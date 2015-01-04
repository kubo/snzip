/*
 * Use SSE4.2 to calculate crc32c.
 *
 * Copyright 2015 Kubo Takehiro <kubo@jiubao.org>
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ''AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of the authors.
 */
#include <stdlib.h>
#include "config.h"
#include "crc32.h"
#include <nmmintrin.h>

uint32_t
calculate_crc32c_sse4_2(uint32_t crc32c,
    const unsigned char *buffer,
    unsigned int length)
{
	size_t quotient;

#if defined(__x86_64) || defined(__x86_64__) || defined(_M_X64)
	quotient = length / 8;
	while (quotient--) {
		crc32c = _mm_crc32_u64(crc32c, *(uint64_t*)buffer);
		buffer += 8;
	}
	if (length & 4) {
		crc32c = _mm_crc32_u32(crc32c, *(uint32_t*)buffer);
		buffer += 4;
	}
#else
	quotient = length / 4;
	while (quotient--) {
		crc32c = _mm_crc32_u32(crc32c, *(uint32_t*)buffer);
		buffer += 4;
	}
#endif
	if (length & 2) {
		crc32c = _mm_crc32_u16(crc32c, *(uint16_t*)buffer);
		buffer += 2;
	}
	if (length & 1) {
		crc32c = _mm_crc32_u8(crc32c, *(uint8_t*)buffer);
	}
	return crc32c;
}
