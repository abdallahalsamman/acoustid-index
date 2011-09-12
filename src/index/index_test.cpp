// Copyright (C) 2011  Lukas Lalinsky
// Distributed under the MIT license, see the LICENSE file for details.

#include <gtest/gtest.h>
#include "util/test_utils.h"
#include "store/ram_directory.h"
#include "store/input_stream.h"
#include "store/output_stream.h"
#include "index.h"
#include "index_writer.h"

using namespace Acoustid;

TEST(IndexTest, OpenEmpty)
{
	RAMDirectory dir;
	Index index(&dir);

	ASSERT_FALSE(dir.fileExists("info_0"));
	ASSERT_THROW(index.open(), IOException);
}

TEST(IndexTest, OpenEmptyCreate)
{
	RAMDirectory dir;
	Index index(&dir);

	ASSERT_FALSE(dir.fileExists("info_0"));
	index.open(true);
	ASSERT_TRUE(dir.fileExists("info_0"));
}

TEST(IndexTest, DeleteUnusedFiled)
{
	RAMDirectory dir;
	Index index(&dir);
	index.open(true);

	ASSERT_TRUE(dir.fileExists("info_0"));
	{
		ScopedPtr<IndexWriter> writer(index.createWriter());
		uint32_t fp[] = { 1, 2, 3 };
		writer->addDocument(1, fp, 3);
		writer->commit();
	}
	qDebug() << dir.listFiles();
	ASSERT_TRUE(dir.fileExists("info_1"));
	ASSERT_FALSE(dir.fileExists("info_0"));
}