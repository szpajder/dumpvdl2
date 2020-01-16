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
#include <stdint.h>
#include <stdbool.h>
#include <libacars/list.h>	// la_list
#include "config.h"		// IS_BIG_ENDIAN
#include "dumpvdl2.h"		// octet_string_t

#define ESIS_HDR_LEN		9
#define ESIS_PDU_TYPE_ESH	2
#define ESIS_PDU_TYPE_ISH	4
/* REDIRECT PDU not used in ATN (ICAO 9705 5.8.2.1.4) */

typedef struct {
	uint8_t pid;
	uint8_t len;
	uint8_t version;
	uint8_t reserved;
#ifdef IS_BIG_ENDIAN
	uint8_t pad:3;
	uint8_t type:5;
#else
	uint8_t type:5;
	uint8_t pad:3;
#endif
	uint8_t holdtime[2];	// not using uint16_t to avoid padding and to match PDU octet layout
	uint8_t cksum[2];
} esis_hdr_t;

typedef struct {
	esis_hdr_t *hdr;
	octet_string_t net_addr;	/* SA for ESH, NET for ISH */
	la_list *options;
	uint16_t holdtime;
	bool err;
} esis_pdu_t;

// esis.c
la_proto_node *esis_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type);
