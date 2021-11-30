// Copyright (C) 2011  Lukas Lalinsky
// Distributed under the MIT license, see the LICENSE file for details.

#include "util/vint.h"
#include "buffered_input_stream.h"

using namespace Acoustid;

BufferedInputStream::BufferedInputStream(size_t bufferSize)
	: m_bufferSize(bufferSize), m_buffer(0), m_start(0), m_position(0), m_length(0)
{
}

BufferedInputStream::~BufferedInputStream()
{
}

size_t BufferedInputStream::bufferSize()
{
	return m_bufferSize;
}

void BufferedInputStream::setBufferSize(size_t bufferSize)
{
	m_bufferSize = bufferSize;
	m_buffer.reset(0);
	m_start += m_position;
	m_position = 0;
	m_length = 0;
}

uint32_t BufferedInputStream::readVInt32()
{
	if (m_position >= m_length) {
		refill();
	}
	if (m_length - m_position >= kMaxVInt32Bytes) {
		// We have enough data in the buffer for any vint32, so we can use an
		// optimized function for reading from memory array.
		uint32_t result;
		ssize_t size = readVInt32FromArray(&m_buffer[m_position], &result);
		if (size == -1) {
			throw IOException("can't read vint32");
		}
		m_position += size;
		return result;
	}
	// We will probably need to refill the buffer in the middle, use the
	// generic implementation and let readByte() handle that.
	return InputStream::readVInt32();
}

void BufferedInputStream::refill()
{
	m_start += m_position;
	m_position = 0;
	if (!m_buffer) {
		m_buffer.reset(new uint8_t[m_bufferSize]);
	}
	m_length = read(m_buffer.get(), m_start, m_bufferSize);
}

size_t BufferedInputStream::position()
{
	return m_start + m_position;
}

void BufferedInputStream::seek(size_t position)
{
	if (m_start <= position && position < (m_start + m_length)) {
		m_position = position - m_start;
	}
	else {
		m_start = position;
		m_position = 0;
		m_length = 0;
	}
}

