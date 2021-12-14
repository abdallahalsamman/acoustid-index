// Copyright (C) 2011  Lukas Lalinsky
// Distributed under the MIT license, see the LICENSE file for details.

#include "index.h"

#include "in_memory_index.h"
#include "index/oplog.h"
#include "index_file_deleter.h"
#include "index_reader.h"
#include "index_writer.h"
#include "segment_data_reader.h"
#include "segment_index_reader.h"
#include "segment_searcher.h"
#include "store/directory.h"
#include "store/input_stream.h"
#include "store/output_stream.h"

using namespace Acoustid;

Index::Index(DirectorySharedPtr dir, bool create)
    : m_mutex(QMutex::Recursive), m_dir(dir), m_open(false), m_hasWriter(false), m_deleter(new IndexFileDeleter(dir)) {
    open(create);
}

Index::~Index() { close(); }

void Index::close() {
    QMutexLocker locker(&m_mutex);
    if (m_open) {
        qDebug() << "Closing index";
        m_writerFuture.waitForFinished();
        setThreadPool(nullptr);
        m_open = false;
    }
}

QThreadPool *Index::threadPool() const { return m_threadPool; }

void Index::setThreadPool(QThreadPool *threadPool) {
    if (threadPool != m_threadPool) {
        if (m_threadPool) {
            m_threadPool->releaseThread();
        }
        if (threadPool) {
            threadPool->reserveThread();
        }
        m_threadPool = threadPool;
    }
}

bool Index::exists(const QSharedPointer<Directory> &dir) {
    if (!dir->exists()) {
        return false;
    }
    return IndexInfo::findCurrentRevision(dir.get()) >= 0;
}

void Index::open(bool create) {
    QMutexLocker locker(&m_mutex);

    if (m_open) {
        return;
    }

    qDebug() << "Opening index";

    if (!m_dir->exists() && !create) {
        throw IndexNotFoundException("index directory does not exist");
    }
    if (!m_info.load(m_dir.data(), true, true)) {
        if (create) {
            m_dir->ensureExists();
            IndexWriter(m_dir, m_info).commit();
            return open(false);
        }
        throw IndexNotFoundException("there is no index in the directory");
    }
    m_oplog = std::make_unique<OpLog>(m_dir->openDatabase("oplog.db"));
    m_stage = std::make_unique<InMemoryIndex>();

    std::vector<OpLogEntry> oplogEntries;
    auto lastOplogId = m_info.attribute("last_oplog_id").toInt();
    while (true) {
        oplogEntries.clear();
        lastOplogId = m_oplog->read(oplogEntries, 100, lastOplogId);
        if (oplogEntries.empty()) {
            break;
        }
        OpBatch batch;
        for (auto oplogEntry : oplogEntries) {
            qDebug() << "Applying oplog entry" << oplogEntry.id();
            batch.add(oplogEntry.op());
        }
        m_stage->applyUpdates(batch);
    }

    m_deleter->incRef(m_info);
    setThreadPool(QThreadPool::globalInstance());
    m_open = true;
}

bool Index::isOpen() const { return m_open; }

IndexInfo Index::acquireInfo() {
    QMutexLocker locker(&m_mutex);
    IndexInfo info = m_info;
    if (m_open) {
        m_deleter->incRef(info);
    }
    // qDebug() << "acquireInfo" << info.files();
    return info;
}

void Index::releaseInfo(const IndexInfo &info) {
    QMutexLocker locker(&m_mutex);
    if (m_open) {
        m_deleter->decRef(info);
    }
    // qDebug() << "releaseInfo" << info.files();
}

void Index::updateInfo(const IndexInfo &oldInfo, const IndexInfo &newInfo, bool updateIndex) {
    QMutexLocker locker(&m_mutex);
    if (m_open) {
        // the infos are opened twice (index + writer), so we need to inc/dec-ref them twice too
        m_deleter->incRef(newInfo);
        if (updateIndex) {
            m_deleter->incRef(newInfo);
            m_deleter->decRef(m_info);
        }
        m_deleter->decRef(oldInfo);
    }
    if (updateIndex) {
        m_info = newInfo;
        for (int i = 0; i < m_info.segmentCount(); i++) {
            assert(!m_info.segment(i).index().isNull());
        }
    }
}

bool Index::containsDocument(uint32_t docId) {
    bool isDeleted;
    if (m_stage->containsDocument(docId, isDeleted)) {
        return !isDeleted;
    }
    return openReader()->containsDocument(docId);
}

QSharedPointer<IndexReader> Index::openReader() {
    QMutexLocker locker(&m_mutex);
    if (!m_open) {
        throw IndexIsNotOpen("index is not open");
    }
    return QSharedPointer<IndexReader>::create(sharedFromThis());
}

QSharedPointer<IndexWriter> Index::openWriter(bool wait, int64_t timeoutInMSecs) {
    QMutexLocker locker(&m_mutex);
    if (!m_open) {
        throw IndexIsNotOpen("index is not open");
    }
    acquireWriterLockInt(wait, timeoutInMSecs);
    return QSharedPointer<IndexWriter>::create(sharedFromThis(), true);
}

void Index::acquireWriterLockInt(bool wait, int64_t timeoutInMSecs) {
    if (m_hasWriter) {
        if (wait) {
            if (m_writerReleased.wait(&m_mutex, QDeadlineTimer(timeoutInMSecs))) {
                m_hasWriter = true;
                return;
            }
        }
        throw IndexIsLocked("there already is an index writer open");
    }
    m_hasWriter = true;
}

void Index::acquireWriterLock(bool wait, int64_t timeoutInMSecs) {
    QMutexLocker locker(&m_mutex);
    acquireWriterLockInt(wait, timeoutInMSecs);
}

void Index::releaseWriterLock() {
    QMutexLocker locker(&m_mutex);
    m_hasWriter = false;
    m_writerReleased.notify_one();
}

std::vector<SearchResult> Index::search(const std::vector<uint32_t> &terms, int64_t timeoutInMSecs) {
    QDeadlineTimer deadline(timeoutInMSecs);
    auto results = m_stage->search(terms, deadline.remainingTime());
    if (deadline.hasExpired()) {
        return results;
    }
    auto results2 = openReader()->search(terms, deadline.remainingTime());
    for (auto result : results2) {
        bool isDeleted;
        if (!m_stage->containsDocument(result.docId(), isDeleted)) {
            results.push_back(result);
        }
    }
    sortSearchResults(results);
    return results;
}

bool Index::hasAttribute(const QString &name) {
    if (m_stage->hasAttribute(name)) {
        return true;
    }
    return info().hasAttribute(name);
}

QString Index::getAttribute(const QString &name) {
    if (m_stage->hasAttribute(name)) {
        return m_stage->getAttribute(name);
    }
    return info().attribute(name);
}

void Index::insertOrUpdateDocument(uint32_t docId, const std::vector<uint32_t> &terms) {
    OpBatch batch;
    batch.insertOrUpdateDocument(docId, terms);
    applyUpdates(batch);
}

void Index::deleteDocument(uint32_t docId) {
    OpBatch batch;
    batch.deleteDocument(docId);
    applyUpdates(batch);
}

void Index::applyUpdates(const OpBatch &batch) {
    m_oplog->write(batch);
    m_stage->applyUpdates(batch);
    /*
    IndexWriter writer(sharedFromThis());
    for (auto op : batch) {
        switch (op.type()) {
            case INSERT_OR_UPDATE_DOCUMENT: {
                auto data = op.data<InsertOrUpdateDocument>();
                writer.insertOrUpdateDocument(data.docId, data.terms);
                break;
            }
            case DELETE_DOCUMENT: {
                auto data = op.data<DeleteDocument>();
                writer.deleteDocument(data.docId);
                break;
            }
            case SET_ATTRIBUTE: {
                auto data = op.data<SetAttribute>();
                writer.setAttribute(data.name, data.value);
                break;
            }
        }
    }
    writer.commit();
    */
}
