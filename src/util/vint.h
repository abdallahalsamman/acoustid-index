// Copyright (C) 2011  Lukas Lalinsky
// Distributed under the MIT license, see the LICENSE file for details.
//
// Some parts of this file are taken from the Protocol Buffers project:
//
// Copyright 2008 Google Inc.  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef ACOUSTID_UTIL_VINT_H_
#define ACOUSTID_UTIL_VINT_H_

#include "common.h"

namespace Acoustid {

static const int kMaxVInt32Bytes = 5;
static const int kMaxVIntBytes = kMaxVInt32Bytes; // only 32-bit vints

// Return the encoded size of a 32-bit varint
inline size_t checkVInt32Size(uint32_t value)
{
	if (value < (1 << 7)) {
		return 1;
	}
	if (value < (1 << 14)) {
		return 2;
	}
	if (value < (1 << 21)) {
		return 3;
	}
	if (value < (1 << 28)) {
		return 4;
	}
	return 5;
}

// Write 32-bit varint to an unbounded array
inline ssize_t writeVInt32ToArray(uint8_t *buffer, uint32_t value)
{
	buffer[0] = static_cast<uint8_t>(value | 0x80);
	if (value >= (1 << 7)) {
		buffer[1] = static_cast<uint8_t>((value >> 7) | 0x80);
		if (value >= (1 << 14)) {
			buffer[2] = static_cast<uint8_t>((value >> 14) | 0x80);
			if (value >= (1 << 21)) {
				buffer[3] = static_cast<uint8_t>((value >> 21) | 0x80);
				if (value >= (1 << 28)) {
					buffer[4] = static_cast<uint8_t>(value >> 28);
					return 5;
				}
				else {
					buffer[3] &= 0x7F;
					return 4;
				}
			}
			else {
				buffer[2] &= 0x7F;
				return 3;
			}
		}
		else {
			buffer[1] &= 0x7F;
			return 2;
		}
	}
	else {
		buffer[0] &= 0x7F;
		return 1;
	}
}

// Read 32-bit varint from an unbounded array
inline ssize_t readVInt32FromArray(const uint8_t *buffer, uint32_t *value)
{
	const uint8_t *ptr = buffer;
	uint32_t b;
	uint32_t result;

	b = *(ptr++); result  = (b & 0x7F)      ; if (!(b & 0x80)) goto done;
	b = *(ptr++); result |= (b & 0x7F) <<  7; if (!(b & 0x80)) goto done;
	b = *(ptr++); result |= (b & 0x7F) << 14; if (!(b & 0x80)) goto done;
	b = *(ptr++); result |= (b & 0x7F) << 21; if (!(b & 0x80)) goto done;
	b = *(ptr++); result |=  b         << 28; if (!(b & 0x80)) goto done;

	// If the input is larger than 32 bits, we still need to read it all
	// and discard the high-order bits.
	for (int i = 0; i < kMaxVIntBytes - kMaxVInt32Bytes; i++) {
	    b = *(ptr++); if (!(b & 0x80)) goto done;
	}

	// We have overrun the maximum size of a varint (10 bytes).  Assume
	// the data is corrupt.
	return -1;

done:
	*value = result;
	return ptr - buffer;
}

}

#endif
