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
#include <libacars/libacars.h>		// la_proto_node
#include <libacars/list.h>		// la_list
#include "dumpvdl2.h"			// octet_string_t
#define BISPDU_HDR_LEN			30U
#define BISPDU_OPEN_VERSION		1

// BISPDU types
#define BISPDU_TYPE_OPEN		1
#define BISPDU_TYPE_UPDATE		2
#define BISPDU_TYPE_ERROR		3
#define BISPDU_TYPE_KEEPALIVE		4
#define BISPDU_TYPE_CEASE		5
#define BISPDU_TYPE_RIBREFRESH		6

// Error codes
#define BISPDU_ERR_OPEN_PDU		1
#define BISPDU_ERR_UPDATE_PDU		2
#define BISPDU_ERR_TIMER_EXPIRED	3
#define BISPDU_ERR_FSM			4
#define BISPDU_ERR_RIB_REFRESH_PDU	5

typedef struct {
	uint8_t pid;
	uint8_t len[2];		// not using uint16_t to avoid padding and to match PDU octet layout
	uint8_t type;
	uint32_t seq, ack;
	uint8_t coff, cavail;
	uint8_t validation[16];
} idrp_hdr_t;

typedef struct {
	idrp_hdr_t *hdr;
	la_list *withdrawn_routes, *path_attributes;
	uint8_t *open_src_rdi;
	octet_string_t *data;
	uint16_t open_holdtime;
	uint16_t open_max_pdu_size;
	uint8_t err_code, err_subcode;
	uint8_t err_fsm_bispdu_type, err_fsm_state;
	uint8_t open_src_rdi_len;
	bool err;
} idrp_pdu_t;

typedef struct {
	char *descr;
	dict *subcodes;
} bispdu_err_t;

// idrp.c
la_proto_node *idrp_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type);
