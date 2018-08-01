/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
 *  Copyright (c) 2017 Tomasz Lemiech <szpajder@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include "dumpvdl2.h"

bitstream_t *bitstream_init(uint32_t len) {
	bitstream_t *ret;
	if(len == 0) return NULL;
	ret = XCALLOC(1, sizeof(bitstream_t));
	ret->buf = XCALLOC(len, sizeof(uint8_t));
	ret->start = ret->end = ret->descrambler_pos = 0;
	ret->len = len;
	return ret;
}

void bitstream_reset(bitstream_t *bs) {
	bs->start = bs->end = bs->descrambler_pos = 0;
}

void bitstream_destroy(bitstream_t *bs) {
	if(bs != NULL) XFREE(bs->buf);
	XFREE(bs);
}

int bitstream_append_msbfirst(bitstream_t *bs, const uint8_t *v, const uint32_t numbytes, const uint32_t numbits) {
	if(bs->end + numbits * numbytes > bs->len)
		return -1;
	for(int i = 0; i < numbytes; i++) {
		uint8_t t = v[i];
		for(int j = numbits - 1; j >= 0; j--)
			bs->buf[bs->end++] = (t >> j) & 0x01;
	}
	return 0;
}

int bitstream_append_lsbfirst(bitstream_t *bs, const uint8_t *bytes, const uint32_t numbytes, const uint32_t numbits) {
	if(bs->end + numbits * numbytes > bs->len)
		 return -1;
	for(int i = 0; i < numbytes; i++) {
		uint8_t t = bytes[i];
		for(int j = 0; j < numbits; j++)
			bs->buf[bs->end++] = (t >> j) & 0x01;
	}
	return 0;
}

int bitstream_read_lsbfirst(bitstream_t *bs, uint8_t *bytes, const uint32_t numbytes, const uint32_t numbits) {
	if(bs->start + numbits * numbytes > bs->end)
		 return -1;
	for(uint32_t i = 0; i < numbytes; i++) {
		bytes[i] = 0x00;
		for(uint32_t j = 0; j < numbits; j++) {
			bytes[i] |= (0x01 & bs->buf[bs->start++]) << j;
		}
	}
	return 0;
}

int bitstream_read_word_msbfirst(bitstream_t *bs, uint32_t *ret, const uint32_t numbits) {
	if(bs->start + numbits > bs->end)
		return -1;
	*ret = 0;
	for(uint32_t i = 0; i < numbits; i++) {
		*ret |= (0x01 & bs->buf[bs->start++]) << (numbits-i-1);
	}
	return 0;
}

void bitstream_descramble(bitstream_t *bs, uint16_t *lfsr) {
	uint8_t bit;
	int i;

	if(bs->descrambler_pos < bs->start)
		bs->descrambler_pos = bs->start;
	for(i = bs->descrambler_pos; i < bs->end; i++) {
		/* LFSR length: 15; feedback polynomial: x^15 + x + 1 */
		bit = ((*lfsr >> 0) ^ (*lfsr >> 14)) & 1;
		*lfsr = (*lfsr >> 1) | (bit << 14);
		bs->buf[i] ^= bit;
	}
	debug_print("descrambled from %u to %u\n", bs->descrambler_pos, bs->end-1);
	bs->descrambler_pos = bs->end;
}

int bitstream_copy_next_frame(bitstream_t *src, bitstream_t *dst) {
	int ones;
	int i, j;
restart:
	ones = 0;
	bitstream_reset(dst);
	for(i = src->start, j = 0; i < src->end; i++, src->start++) {
		if(src->buf[i] == 0x0 && ones == 5) {		// stuffed 0 bit - skip it
			ones = 0;
			continue;
		} else if(src->buf[i] == 0x1) {
			ones++;
			if(ones > 6) {				// 7 ones - invalid bit sequence
				debug_print("%s", "Invalid bit stuffing sequence\n");
				return -1;
			}
		}
		dst->buf[j] = src->buf[i];
		if(src->buf[i] == 0x0) {
			if(ones == 6) {				// frame boundary flag (0x7e)
				if(j == 7) {			// move past the initial flag
					src->start++;
					debug_print("%s", "Initial flag found, restarting\n");
					goto restart;
				} else {
					if(j < 7) {
						debug_print("%s", "Invalid bit sequence - 6 ones at the start of the stream\n");
						return -1;
					}
					dst->end = j - 7;	// remove trailing flag from the result
					src->start++;
					break;
				}
			}
			ones = 0;
		}
		j++; dst->end++;
	}
	debug_print("dst len: %u, next src read at %u, remaining src length: %u\n",
		dst->end - dst->start, src->start, src->end - src->start);
	return (src->start < src->end ? 1 : 0);
}

uint32_t reverse(uint32_t v, int numbits) {
	uint32_t r = v; // r will be reversed bits of v; first get LSB of v
	int s = sizeof(v) * CHAR_BIT - 1; // extra shift needed at end

	for (v >>= 1; v; v >>= 1) {
		r <<= 1;
		r |= v & 1;
		s--;
	}
	r <<= s; // shift when v's highest bits are zero
	r >>= 32 - numbits;
	return r;
}
