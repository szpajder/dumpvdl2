#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include "rtlvdl2.h"

bitstream_t *bitstream_init(uint32_t len) {
	bitstream_t *ret;
	if(len == 0) return NULL;
	ret = calloc(1, sizeof(bitstream_t));
	if(ret == NULL) {
		debug_print("calloc 1 failed %s\n", strerror(errno));
		return NULL;
	}
	ret->buf = calloc(len, sizeof(uint8_t));
	if(ret->buf == NULL) {
		free(ret);
		debug_print("calloc 2 failed %s\n", strerror(errno));
		return NULL;
	}
	ret->start = ret->end = ret->descrambler_pos = 0;
	ret->len = len;
	return ret;
}

void bitstream_reset(bitstream_t *bs) {
	bs->start = bs->end = bs->descrambler_pos = 0;
}

int bitstream_append_msbfirst(bitstream_t *bs, const uint8_t *v, const uint32_t numbytes, const uint32_t numbits) {
	assert(bs);
	assert(numbits > 0 || numbits <= CHAR_BIT);
	assert(numbytes > 0);
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
	assert(bs);
	assert(numbytes > 0);
	if(bs->end + numbits * numbytes > bs->len)
		 return -1;
	for(int i = 0; i < numbytes; i++) {
		uint8_t t = bytes[i];
		for(int j = 0; j < numbits; j++)
			bs->buf[bs->end++] = (t >> j) & 0x01;
	}
	return 0;
}

int bitstream_read_msbfirst(bitstream_t *bs, uint8_t *bytes, const uint32_t numbytes, const uint32_t numbits) {
	assert(bs);
	assert(numbytes > 0);
	if(bs->start + numbits * numbytes > bs->end)
		 return -1;
	for(uint32_t i = 0; i < numbytes; i++) {
		bytes[i] = 0x00;
		for(uint32_t j = 0; j < numbits; j++) {
			bytes[i] |= (0x01 & bs->buf[bs->start++]) << (numbits-j-1);
		}
	}
	return 0;
}

int bitstream_read_msbfirst2(bitstream_t *bs, uint8_t *bytes, const uint32_t numreadbits, const uint32_t numbits) {
	assert(bs);
	assert(numreadbits > 0);
	debug_print("start=%u + numreadbits=%u = %u (bs->end=%u)\n", bs->start , numreadbits, bs->start+numreadbits, bs->end);
	if(bs->start + numreadbits > bs->end)
		 return -1;
	int n = 0, j = 0;
	bytes[0] = 0x00;
	for(uint32_t i = 0; i < numreadbits; i++) {
		bytes[n] |= (0x01 & bs->buf[bs->start++]) << (numbits-j-1);
		j++;
		if(j == numbits) {
			j = 0;
			bytes[++n] = 0x00;
		}
	}
	return 0;
}

int bitstream_read_msbfirst_pad(bitstream_t *bs, uint8_t *bytes, const uint32_t numbytes, const uint32_t numbits) {
	assert(bs);
	assert(numbytes > 0);
	int npadding = numbits * numbytes - (bs->end - bs->start);
	if(npadding < 0) npadding = 0;
	for(uint32_t i = 0; i < numbytes; i++) {
		bytes[i] = 0x00;
		uint32_t j = 0;
		while(bs->start < bs->end && j < numbits) {
			bytes[i] |= (0x01 & bs->buf[bs->start++]) << (numbits-j-1);
			j++;
		}
	}
// FIXME: trzeba wyzerowac bajty wyjsciowe az do numbytes-1
	return npadding;
}

int bitstream_read_lsbfirst(bitstream_t *bs, uint8_t *bytes, const uint32_t numbytes, const uint32_t numbits) {
	assert(bs);
	assert(numbytes > 0);
// FIXME: padding zerami, gdy zabraknie danych?
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
	assert(bs);
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

	assert(bs);
	assert(bs->descrambler_pos < bs->end);
	if(bs->descrambler_pos < bs->start)
		bs->descrambler_pos = bs->start;
	for(i = bs->descrambler_pos; i < bs->end; i++) {
		/* LFSR length: 15; feedback polynomial: x^15 + x + 1 */
		bit  = ((*lfsr >> 0) ^ (*lfsr >> 14)) & 1;
		*lfsr =  (*lfsr >> 1) | (bit << 14);
//		debug_print("lfsr: bit: %u state: %x\n", bit, *lfsr);
		bs->buf[i] ^= bit;
	}
	debug_print("descrambled from %u to %u\n", bs->descrambler_pos, bs->end-1);
	bs->descrambler_pos = bs->end;
}

int bitstream_hdlc_unstuff(bitstream_t *bs) {
	int ones = 0;
	int i, j;
	for(i = j = bs->start; i < bs->end; i++) {
		if(bs->buf[i] == 0x1) {
			ones++;
			if(ones > 6)		// 7 ones - invalid bit sequence
				return -1;
		} else {				// bs->buf[i] == 0
			if(ones == 5) {		// stuffed 0 bit - skip it
				ones = 0;
				continue;
			}
			ones = 0;
		}
		bs->buf[j++] = bs->buf[i];
	}
	debug_print("Unstuffed %u bits\n", bs->end - j);
	bs->end = j;
	return 0;
}

uint32_t reverse(uint32_t v, int numbits) {
	uint32_t vv = v;
	uint32_t r = v; // r will be reversed bits of v; first get LSB of v
	int s = sizeof(v) * CHAR_BIT - 1; // extra shift needed at end

	for (v >>= 1; v; v >>= 1) {   
	  r <<= 1;
	  r |= v & 1;
	  s--;
	}
	r <<= s; // shift when v's highest bits are zero
	r >>= 32 - numbits;
	debug_print("0x%x -> 0x%x\n", vv, r);
	return r;
}

// vim: ts=4
