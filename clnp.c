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
#include <stdint.h>
#include <string.h>
#include "dumpvdl2.h"
#include "clnp.h"
#include "idrp.h"

static void parse_clnp_pdu_payload(clnp_pdu_t *pdu, uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	if(len == 0)
		goto clnp_pdu_payload_unparsed;
	pdu->proto = *buf;
	switch(*buf) {
	case SN_PROTO_ESIS:
// not implemented yet
		break;
	case SN_PROTO_IDRP:
		pdu->data = parse_idrp_pdu(buf, len, msg_type);
		break;
	case SN_PROTO_CLNP:
		debug_print("%s", "CLNP inside CLNP? Bailing out to avoid loop\n");
		break;
	}
	if(pdu->data != NULL) {
		pdu->data_valid = 1;
		return;
	}
clnp_pdu_payload_unparsed:
	pdu->data_valid = 0;
	pdu->data = buf;
	pdu->datalen = len;
}

clnp_pdu_t *parse_clnp_pdu(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	static clnp_pdu_t *pdu = NULL;
	if(len < CLNP_MIN_LEN) {
		debug_print("Too short (len %u < min len %u)\n", len, CLNP_MIN_LEN);
		return NULL;
	}
	if(pdu == NULL) {
		pdu = XCALLOC(1, sizeof(clnp_pdu_t));
	} else {
		memset(pdu, 0, sizeof(clnp_pdu_t));
	}

	uint32_t hdrlen = buf[1];
	if(len < hdrlen) {
		debug_print("header truncated: buf_len %u < hdr_len %u\n", len, hdrlen);
		return NULL;
	}
	buf += hdrlen; len -= hdrlen;
	parse_clnp_pdu_payload(pdu, buf, len, msg_type);
	return pdu;
}

clnp_pdu_t *parse_clnp_compressed_init_pdu(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	static clnp_pdu_t *pdu = NULL;
	if(len < CLNP_COMPRESSED_INIT_MIN_LEN) {
		debug_print("Too short (len %u < min len %u)\n", len, CLNP_COMPRESSED_INIT_MIN_LEN);
		return NULL;
	}
	if(pdu == NULL) {
		pdu = XCALLOC(1, sizeof(clnp_pdu_t));
	} else {
		memset(pdu, 0, sizeof(clnp_pdu_t));
	}

	uint32_t hdrlen = CLNP_COMPRESSED_INIT_MIN_LEN;
	if(buf[3] & 0x80) hdrlen += 1;		// EXP flag = 1 means localRef/B octet is present
	if(buf[0] & 0x10) hdrlen += 2;		// odd PDU type means PDU identifier is present

	debug_print("buf[0]: %02x buf[3]: %02x hdrlen: %u\n", buf[0], buf[3], hdrlen);
	if(len < hdrlen) {
		debug_print("header truncated: buf_len %u < hdr_len %u\n", len, hdrlen);
		return NULL;
	}
	buf += hdrlen; len -= hdrlen;
	parse_clnp_pdu_payload(pdu, buf, len, msg_type);
	return pdu;
}

static void output_clnp_pdu(clnp_pdu_t *pdu) {
	if(pdu == NULL) {
		fprintf(outf, "-- NULL PDU\n");
		return;
	}
	switch(pdu->proto) {
	case SN_PROTO_ESIS:
		if(pdu->data_valid) {
//			output_esis(pdu->data);
		} else {
//			fprintf(outf, "-- Unparseable ES-IS PDU\n");
			fprintf(outf, "ES-IS PDU:\n");
			output_raw(pdu->data, pdu->datalen);
		}
		break;
	case SN_PROTO_IDRP:
		if(pdu->data_valid) {
			output_idrp(pdu->data);
		} else {
			fprintf(outf, "-- Unparseable IDRP PDU\n");
			output_raw(pdu->data, pdu->datalen);
		}
		break;
	case SN_PROTO_CLNP:
		fprintf(outf, "-- Nested CLNP PDU - ignored\n");
		output_raw(pdu->data, pdu->datalen);
		break;
	default:
		fprintf(outf, "Unknown protocol 0x%02x\n", pdu->proto);
		output_raw(pdu->data, pdu->datalen);
	}
}

void output_clnp(clnp_pdu_t *pdu) {
	fprintf(outf, "CLNP PDU:\n");
	output_clnp_pdu(pdu);
}

void output_clnp_compressed(clnp_pdu_t *pdu) {
	fprintf(outf, "CLNP PDU, compressed header:\n");
	output_clnp_pdu(pdu);
}
