/*
 *  This file is a part of dumpvdl2
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "asn1/FANSATCDownlinkMessage.h"	// asn_DEF_FANSATCDownlinkMessage
#include "asn1/FANSATCUplinkMessage.h"		// asn_DEF_FANSATCUplinkMessage
#include "asn1/asn_application.h"		// asn_fprint()
#include "dumpvdl2.h"				// outf
#include "asn1-util.h"				// asn1_decode_as()
#include "cpdlc.h"

static char const * const cpdlc_msgid_descr_table[CPDLC_MSGID_CNT] = {
	[CPDLC_MSG_UNKNOWN]	= "Unknown message",
	[CPDLC_MSG_CR1]		= "CPDLC Connect Request",
	[CPDLC_MSG_CC1]		= "CPDLC Connect Confirm",
	[CPDLC_MSG_DR1]		= "CPDLC Disconnect Request",
	[CPDLC_MSG_AT1]		= "CPDLC Message"
};

cpdlc_msg_t *cpdlc_parse_msg(cpdlc_msgid_t msgid, uint8_t *buf, size_t len, uint32_t *msg_type) {
	if(buf == NULL)
		return NULL;
	if(len < CPDLC_CRC_LEN) {
		debug_print("message too short: %zu < %d\n", len, CPDLC_CRC_LEN);
		return NULL;
	}
// cut off CRC
	len -= CPDLC_CRC_LEN;

	static cpdlc_msg_t msg;
	if(msg.asn_type != NULL) {
		msg.asn_type->free_struct(msg.asn_type, msg.data, 0);
	}
	memset(&msg, 0, sizeof(msg));
	msg.id = msgid;

	if(len == 0) {
// empty payload is not an error
		debug_print("%s", "Empty FANS-1/A message, decoding skipped\n");
		return &msg;
	}
	if(*msg_type & MSGFLT_SRC_GND)
		msg.asn_type = &asn_DEF_FANSATCUplinkMessage;
	else if(*msg_type & MSGFLT_SRC_AIR)
		msg.asn_type = &asn_DEF_FANSATCDownlinkMessage;
	assert(msg.asn_type != NULL);

	debug_print("Decoding as %s, len: %zu\n", msg.asn_type->name, len);

	if(asn1_decode_as(msg.asn_type, &msg.data, buf, len) != 0)
		msg.err = 1;

	return &msg;
}

void cpdlc_output_msg(cpdlc_msg_t *msg) {
	if(msg == NULL) {
		fprintf(outf, "-- NULL FANS-1/A message\n");
		return;
	}
	fprintf(outf, "FANS-1/A %s:\n", cpdlc_msgid_descr_table[msg->id]);
	if(msg->err) {
		fprintf(outf, "-- Unparseable FANS-1/A message\n");
		return;
	}
	if(msg->asn_type != NULL) {
		if(msg->data != NULL)
			asn_fprint(outf, msg->asn_type, msg->data);
		else
			fprintf(outf, "%s: <empty PDU>\n", msg->asn_type->name);
	}
}
