/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
 *  Copyright (c) 2017-2018 Tomasz Lemiech <szpajder@gmail.com>
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
#include "asn1/constr_TYPE.h"	// asn_TYPE_descriptor_t

#define CPDLC_CRC_LEN 2

typedef enum {
	CPDLC_MSG_UNKNOWN,
	CPDLC_MSG_CR1,
	CPDLC_MSG_CC1,
	CPDLC_MSG_DR1,
	CPDLC_MSG_AT1
} cpdlc_msgid_t;
#define CPDLC_MSGID_CNT 5

typedef struct {
	asn_TYPE_descriptor_t *asn_type;
	void *data;
	cpdlc_msgid_t id;
	uint8_t err;
} cpdlc_msg_t;

// cpdlc.c
cpdlc_msg_t *cpdlc_parse_msg(cpdlc_msgid_t msgid, uint8_t *buf, size_t len, uint32_t *msg_type);
void cpdlc_output_msg(cpdlc_msg_t *msg);
