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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <libacars/libacars.h>		// la_proto_node
#include <libacars/vstring.h>		// la_vstring
#include "dumpvdl2.h"
#include "clnp.h"
#include "idrp.h"
#include "cotp.h"

// Forward declarations
la_type_descriptor const proto_DEF_clnp_compressed_init_pdu;
la_type_descriptor const proto_DEF_clnp_pdu;

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
	default:
// assume X.224 COTP TPDU
		pdu->data = parse_cotp_concatenated_pdu(buf, len, msg_type);
		if(pdu->data != NULL)
			pdu->proto = SN_PROTO_COTP;
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

la_proto_node *clnp_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	clnp_pdu_t *pdu = XCALLOC(1, sizeof(clnp_pdu_t));
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_clnp_pdu;
	node->data = pdu;
	node->next = NULL;

	pdu->err = true;	// fail-safe default
	if(len < CLNP_MIN_LEN) {
		debug_print("Too short (len %u < min len %u)\n", len, CLNP_MIN_LEN);
		goto end;
	}

	uint32_t hdrlen = buf[1];
	if(len < hdrlen) {
		debug_print("header truncated: buf_len %u < hdr_len %u\n", len, hdrlen);
		goto end;
	}
	buf += hdrlen; len -= hdrlen;
	parse_clnp_pdu_payload(pdu, buf, len, msg_type);
	pdu->err = false;
end:
	return node;
}

la_proto_node *clnp_compressed_init_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	clnp_pdu_t *pdu = XCALLOC(1, sizeof(clnp_pdu_t));
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_clnp_compressed_init_pdu;
	node->data = pdu;
	node->next = NULL;

	pdu->err = true;	// fail-safe default
	if(len < CLNP_COMPRESSED_INIT_MIN_LEN) {
		debug_print("Too short (len %u < min len %u)\n", len, CLNP_COMPRESSED_INIT_MIN_LEN);
		goto end;
	}

	uint32_t hdrlen = CLNP_COMPRESSED_INIT_MIN_LEN;
	if(buf[3] & 0x80) hdrlen += 1;		// EXP flag = 1 means localRef/B octet is present
	if(buf[0] & 0x10) hdrlen += 2;		// odd PDU type means PDU identifier is present

	debug_print("buf[0]: %02x buf[3]: %02x hdrlen: %u\n", buf[0], buf[3], hdrlen);
	if(len < hdrlen) {
		debug_print("header truncated: buf_len %u < hdr_len %u\n", len, hdrlen);
		goto end;
	}
	buf += hdrlen; len -= hdrlen;
	parse_clnp_pdu_payload(pdu, buf, len, msg_type);
	pdu->err = false;
end:
	return node;
}

static void output_clnp_pdu(la_vstring * const vstr, clnp_pdu_t *pdu, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(pdu != NULL);
	ASSERT(indent >= 0);

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
	case SN_PROTO_COTP:
		if(pdu->data_valid) {
			output_cotp_concatenated_pdu(pdu->data);
		} else {
			fprintf(outf, "-- Unparseable COTP TPDU\n");
			output_raw(pdu->data, pdu->datalen);
		}
		break;
	default:
		fprintf(outf, "Unknown protocol 0x%02x\n", pdu->proto);
		output_raw(pdu->data, pdu->datalen);
	}
}

void clnp_pdu_format_text(la_vstring * const vstr, void const * const data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	CAST_PTR(pdu, clnp_pdu_t *, data);
	if(pdu->err == true) {
		LA_ISPRINTF(vstr, indent, "%s", "-- Unparseable CLNP PDU\n");
		return;
	}
	LA_ISPRINTF(vstr, indent, "%s", "CLNP PDU:\n");
	output_clnp_pdu(vstr, pdu, indent+1);
}

void clnp_compressed_pdu_format_text(la_vstring * const vstr, void const * const data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	CAST_PTR(pdu, clnp_pdu_t *, data);
	if(pdu->err == true) {
		LA_ISPRINTF(vstr, indent, "%s", "-- Unparseable CLNP PDU\n");
		return;
	}
	LA_ISPRINTF(vstr, indent, "%s", "CLNP PDU, compressed header:\n");
	output_clnp_pdu(vstr, pdu, indent+1);
}

la_type_descriptor const proto_DEF_clnp_compressed_init_pdu = {
	.format_text = clnp_compressed_pdu_format_text,
	.destroy = NULL,
};

la_type_descriptor const proto_DEF_clnp_pdu = {
	.format_text = clnp_pdu_format_text,
	.destroy = NULL,
};
