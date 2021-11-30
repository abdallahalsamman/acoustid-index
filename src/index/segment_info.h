// Copyright (C) 2011  Lukas Lalinsky
// Distributed under the MIT license, see the LICENSE file for details.

#ifndef ACOUSTID_SEGMENT_INFO_H_
#define ACOUSTID_SEGMENT_INFO_H_

#include <QSharedData>
#include <QSharedDataPointer>

#include "common.h"
#include "segment_index.h"

namespace Acoustid {

class SegmentDocs;

// Internal, do not use.
class SegmentInfoData : public QSharedData {
 public:
    SegmentInfoData(int id = 0, size_t blockCount = 0, uint32_t lastKey = 0, uint32_t checksum = 0, SegmentIndexSharedPtr index = SegmentIndexSharedPtr())
        : id(id), blockCount(blockCount), lastKey(lastKey), checksum(checksum), index(index) {}
    SegmentInfoData(const SegmentInfoData& other)
        : QSharedData(other), id(other.id), blockCount(other.blockCount), lastKey(other.lastKey), checksum(other.checksum), index(other.index) {}
    ~SegmentInfoData() {}

    int id;
    size_t blockCount;
    uint32_t lastKey;
    uint32_t checksum;
    SegmentIndexSharedPtr index;
    std::shared_ptr<SegmentDocs> docs;
};

class SegmentInfo {
 public:
    SegmentInfo(int id = 0, size_t blockCount = 0, uint32_t lastKey = 0, uint32_t checksum = 0, SegmentIndexSharedPtr index = SegmentIndexSharedPtr())
        : d(new SegmentInfoData(id, blockCount, lastKey, checksum, index)) {}

    QString name() const { return QString("segment_%1").arg(id()); }

    QString indexFileName() const { return name() + ".fii"; }

    QString dataFileName() const { return name() + ".fid"; }

    QString docsFileName() const { return name() + ".docs"; }

    void setId(int id) { d->id = id; }

    int id() const { return d->id; }

    uint32_t lastKey() const { return d->lastKey; }

    void setLastKey(uint32_t lastKey) { d->lastKey = lastKey; }

    uint32_t checksum() const { return d->checksum; }

    void setChecksum(uint32_t checksum) { d->checksum = checksum; }

    size_t blockCount() const { return d->blockCount; }

    void setBlockCount(size_t blockCount) { d->blockCount = blockCount; }

    SegmentIndexSharedPtr index() const { return d->index; }

    void setIndex(SegmentIndexSharedPtr index) { d->index = index; }

    std::shared_ptr<SegmentDocs> docs() const { return d->docs; }

    void setDocs(const std::shared_ptr<SegmentDocs>& docs) { d->docs = docs; }

    QList<QString> files() const;

 private:
    QSharedDataPointer<SegmentInfoData> d;
};

typedef QList<SegmentInfo> SegmentInfoList;

}  // namespace Acoustid

#endif
