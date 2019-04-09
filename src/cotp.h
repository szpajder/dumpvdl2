/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
 *  Copyright (c) 2017-2019 Tomasz Lemiech <szpajder@gmail.com>
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
#include <glib.h>
#include <libacars/libacars.h>		// la_proto_node

// These defines apply to upper nibble of the TPDU code only
#define COTP_TPDU_CR	0xe0
#define COTP_TPDU_CC	0xd0
#define COTP_TPDU_DR	0x80
#define COTP_TPDU_DC	0xc0
#define COTP_TPDU_DT	0xf0
#define COTP_TPDU_ED	0x10
#define COTP_TPDU_AK	0x60
#define COTP_TPDU_EA	0x20
#define COTP_TPDU_RJ	0x50
#define COTP_TPDU_ER	0x70

typedef struct {
	void *data;
	tlv_list_t *options;
	uint32_t datalen;
	uint8_t code;
// protocol class for CR/CC, disconnect reason for DR, reject cause for ER
	uint8_t class_or_status;
	uint8_t data_valid;		// higher level PDU has been parsed successfully
	bool err;
} cotp_pdu_t;

// cotp.c
la_proto_node *cotp_concatenated_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type);
