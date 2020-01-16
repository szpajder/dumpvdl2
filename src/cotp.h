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
	la_list *variable_part_params;
	uint32_t tpdu_seq;	// TPDU sequence number (valid for DT, ED, AK)
	uint16_t src_ref, dst_ref;
	uint16_t credit;	// (credit for AK, RJ, initial credit for CR, CC)
	int16_t x225_xport_disc_reason;
	uint8_t code;
	uint8_t roa;		// Request of Acknowledgment (valid for DT)
	uint8_t eot;		// last fragment of TSDU (valid for DT, ED)
	uint8_t	options;	// Option flags (valid for CR)
// protocol class for CR/CC, disconnect reason for DR, reject cause for ER
	uint8_t class_or_disc_reason;
	bool extended;		// TPDU format - normal or extended
	bool err;
} cotp_pdu_t;

// cotp.c
la_proto_node *cotp_concatenated_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type);
