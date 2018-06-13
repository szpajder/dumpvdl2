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
#ifndef _AVLC_H
#define _AVLC_H 1
#include <endian.h>
#include <stdint.h>
#include <time.h>
#define MIN_AVLC_LEN	11
#define AVLC_FLAG	0x7e

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define BSHIFT 24
#elif __BYTE_ORDER == __BIG_ENDIAN
#define BSHIFT 0
#else
#error Unsupported endianness
#endif

typedef union {
	uint32_t val;
	struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
		uint32_t addr:24;
		uint8_t type:3;
		uint8_t status:1;
#elif __BYTE_ORDER == __BIG_ENDIAN
		uint8_t status:1;
		uint8_t type:3;
		uint32_t addr:24;
#endif
	} a_addr;
} avlc_addr_t;

// X.25 control field
typedef union {
	uint8_t val;
	struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
		uint8_t type:1;
		uint8_t send_seq:3;
		uint8_t poll:1;
		uint8_t recv_seq:3;
#elif __BYTE_ORDER == __BIG_ENDIAN
		uint8_t recv_seq:3;
		uint8_t poll:1;
		uint8_t send_seq:3;
		uint8_t type:1;
#endif
	} I;
	struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
		uint8_t type:2;
		uint8_t sfunc:2;
		uint8_t pf:1;
		uint8_t recv_seq:3;
#elif __BYTE_ORDER == __BIG_ENDIAN
		uint8_t recv_seq:3;
		uint8_t pf:1;
		uint8_t sfunc:2;
		uint8_t type:2;
#endif
	} S;
	struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
		uint8_t type:2;
		uint8_t mfunc:6;
#elif __BYTE_ORDER == __BIG_ENDIAN
		uint8_t mfunc:6;
		uint8_t type:2;
#endif
	} U;
} lcf_t;

#define IS_I(lcf) ((lcf).val & 0x1) == 0x0
#define IS_S(lcf) ((lcf).val & 0x3) == 0x1
#define IS_U(lcf) ((lcf).val & 0x3) == 0x3
#define U_MFUNC(lcf) (lcf).U.mfunc & 0x3b
#define U_PF(lcf) ((lcf).U.mfunc >> 2) & 0x1

#define UI	0x00
#define DM	0x03
#define DISC	0x10
#define FRMR	0x21
#define XID	0x2b
#define TEST	0x38

#define ADDRTYPE_AIRCRAFT	1
#define ADDRTYPE_GS_ADM		4
#define ADDRTYPE_GS_DEL		5
#define ADDRTYPE_ALL		7

enum avlc_protocols { PROTO_X25, PROTO_ACARS, PROTO_UNKNOWN };
typedef struct {
	time_t t;
	avlc_addr_t src;
	avlc_addr_t dst;
	lcf_t lcf;
	enum avlc_protocols proto;
	uint32_t datalen;
	uint8_t data_valid;
	void *data;
} avlc_frame_t;

typedef struct {
	uint8_t *buf;
	uint32_t len;
	uint32_t freq;
	float mag_frame, mag_nf;
} avlc_frame_qentry_t;
#endif // !_AVLC_H
