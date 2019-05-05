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
#include "esis.h"
#include "idrp.h"
#include "cotp.h"

// Forward declarations
la_type_descriptor const proto_DEF_clnp_compressed_init_data_pdu;
la_type_descriptor const proto_DEF_clnp_pdu;

static la_proto_node *parse_clnp_pdu_payload(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	if(len == 0) {
		return NULL;
	}
	switch(*buf) {
	case SN_PROTO_ESIS:
		return esis_pdu_parse(buf, len, msg_type);
	case SN_PROTO_IDRP:
		return idrp_pdu_parse(buf, len, msg_type);
	case SN_PROTO_CLNP:
		debug_print("%s", "CLNP inside CLNP? Bailing out to avoid loop\n");
		break;
	default:
// assume X.224 COTP TPDU
		return cotp_concatenated_pdu_parse(buf, len, msg_type);
	}
	return unknown_proto_pdu_new(buf, len);
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
	node->next = parse_clnp_pdu_payload(buf, len, msg_type);
	pdu->err = false;
end:
	return node;
}

la_proto_node *clnp_compressed_init_data_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	clnp_compressed_init_data_pdu_t *pdu = XCALLOC(1, sizeof(clnp_compressed_init_data_pdu_t));
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_clnp_compressed_init_data_pdu;
	node->data = pdu;
	node->next = NULL;

	pdu->err = true;	// fail-safe default
	if(len < CLNP_COMPRESSED_INIT_MIN_LEN) {
		debug_print("Too short (len %u < min len %u)\n", len, CLNP_COMPRESSED_INIT_MIN_LEN);
		goto end;
	}

	uint32_t hdrlen = CLNP_COMPRESSED_INIT_MIN_LEN;
	CAST_PTR(hdr, clnp_compressed_init_data_pdu_hdr_t *, buf);
	pdu->hdr = hdr;
	if(hdr->exp != 0) hdrlen += 1;		// EXP flag = 1 means localRef/B octet is present
	if(hdr->type & 1) hdrlen += 2;		// odd PDU type means PDU identifier is present

	debug_print("hdrlen: %u type: %02x prio: %02x lifetime: %02x flags: %02x exp: %d lref_a: %02x\n",
		hdrlen, hdr->type, hdr->priority, hdr->lifetime, hdr->flags.val, hdr->exp, hdr->lref_a);

	if(len < hdrlen) {
		debug_print("header truncated: buf_len %u < hdr_len %u\n", len, hdrlen);
		goto end;
	}
	buf += 4; len -= 4;
	if(hdr->exp != 0) {
		debug_print("lref_b: %02x\n", buf[0]);
		pdu->lref = ((uint16_t)(hdr->lref_a) << 8) | (uint16_t)buf[0];
		buf++; len--;
	} else {
		pdu->lref = (uint16_t)(hdr->lref_a);
	}
	if(hdr->type & 1) {
		pdu->pdu_id = extract_uint16_msbfirst(buf);
		pdu->pdu_id_present = true;
		buf += 2; len -= 2;
	} else {
		pdu->pdu_id_present = false;
	}
	node->next = parse_clnp_pdu_payload(buf, len, msg_type);
	pdu->err = false;
end:
	return node;
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
}

void clnp_compressed_init_data_pdu_format_text(la_vstring * const vstr, void const * const data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	CAST_PTR(pdu, clnp_compressed_init_data_pdu_t *, data);
	if(pdu->err == true) {
		LA_ISPRINTF(vstr, indent, "%s", "-- Unparseable CLNP Data PDU (compressed)\n");
		return;
	}
	LA_ISPRINTF(vstr, indent, "%s", "CLNP Data PDU (compressed):\n");
	indent++;
	LA_ISPRINTF(vstr, indent, "LRef: 0x%x Prio: %u Lifetime: %u Flags: 0x%02x\n",
		pdu->lref, pdu->hdr->priority, pdu->hdr->lifetime, pdu->hdr->flags.val);
	if(pdu->pdu_id_present) {
		LA_ISPRINTF(vstr, indent, "PDU Id: %u\n", pdu->pdu_id);
	}
}

la_type_descriptor const proto_DEF_clnp_compressed_init_data_pdu = {
	.format_text = clnp_compressed_init_data_pdu_format_text,
	.destroy = NULL,
};

la_type_descriptor const proto_DEF_clnp_pdu = {
	.format_text = clnp_pdu_format_text,
	.destroy = NULL,
};
