/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2020 Tomasz Lemiech <szpajder@gmail.com>
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
#include <stdint.h>
#include <sys/time.h>               // struct timeval
#include <glib.h>                   // GAsyncQueue
#include <libacars/reassembly.h>    // la_reasm_ctx
#include "config.h"                 // IS_BIG_ENDIAN

typedef union {
	uint32_t val;
	struct {
#ifdef IS_BIG_ENDIAN
		uint8_t status:1;
		uint8_t type:3;
		uint32_t addr:24;
#else
		uint32_t addr:24;
		uint8_t type:3;
		uint8_t status:1;
#endif
	} a_addr;
} avlc_addr_t;

typedef struct {
	uint8_t *buf;
	uint32_t len;
	uint32_t freq;
	uint32_t synd_weight;
	uint32_t datalen_octets;
	float frame_pwr, mag_nf;
	float ppm_error;
	int num_fec_corrections;
	int idx;
	struct timeval burst_timestamp;
} avlc_frame_qentry_t;

uint32_t parse_dlc_addr(uint8_t *buf);
la_proto_node *avlc_parse(avlc_frame_qentry_t *q, uint32_t *msg_type, la_reasm_ctx *reasm_ctx);
#endif // !_AVLC_H
