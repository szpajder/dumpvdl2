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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <glib.h>
#include <libacars/libacars.h>		// la_proto_node
#include <libacars/vstring.h>		// la_vstring
#include "dumpvdl2.h"
#include "tlv.h"
#include "cotp.h"
#include "icao.h"

// Forward declaration
la_type_descriptor const proto_DEF_cotp_concatenated_pdu;

typedef struct {
	cotp_pdu_t *pdu;
	la_proto_node *next_node;
	int consumed;
} cotp_pdu_parse_result;

static cotp_pdu_parse_result cotp_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	cotp_pdu_parse_result r = { NULL, NULL, 0 };
	cotp_pdu_t *pdu = XCALLOC(1, sizeof(cotp_pdu_t));
	r.pdu = pdu;

	pdu->err = true;	// fail-safe default
	int final_pdu = 0;
	uint8_t *ptr = buf;
	uint32_t remaining = len;

	uint8_t li = ptr[0];
	ptr++; remaining--;
	if(li == 0 || li == 255) {
		debug_print("invalid header length indicator: %u\n", li);
		goto fail;
	}
	if(remaining < li) {
		debug_print("header truncated: len %u < li %u\n", remaining, li);
		goto fail;
	}
	uint8_t code = ptr[0];
	switch(code & 0xf0) {
	case COTP_TPDU_CR:
	case COTP_TPDU_CC:
	case COTP_TPDU_AK:
	case COTP_TPDU_RJ:
		pdu->code = code & 0xf0;
		break;
	case COTP_TPDU_DT:
		pdu->code = code & 0xfe;
		break;
	default:
		pdu->code = code;
	}
	debug_print("TPDU code: 0x%02x\n", pdu->code);
	switch(pdu->code) {
	case COTP_TPDU_CR:
	case COTP_TPDU_CC:
	case COTP_TPDU_DR:
		if(remaining < 6) {
			debug_print("Truncated TPDU: code: 0x%02x len: %u\n", pdu->code, remaining);
			goto fail;
		}
		if(pdu->code == COTP_TPDU_DR)
			pdu->class_or_status = ptr[5];		// reason
		else
			pdu->class_or_status = ptr[5] >> 4;	// protocol class
		final_pdu = 1;
		break;
	case COTP_TPDU_ER:
		if(remaining < 4) {
			debug_print("Truncated TPDU: code: 0x%02x len: %u\n", pdu->code, remaining);
			goto fail;
		}
		pdu->class_or_status = ptr[3];			// reject cause
		break;
	case COTP_TPDU_DT:
	case COTP_TPDU_ED:
		final_pdu = 1;
		break;
	case COTP_TPDU_DC:
	case COTP_TPDU_AK:
	case COTP_TPDU_EA:
	case COTP_TPDU_RJ:
		break;
	default:
		debug_print("Unknown TPDU code 0x%02x\n", pdu->code);
		goto fail;
	}
	if(final_pdu) {
// user data is allowed in this PDU; if it's there, try to parse it
		ptr += li; remaining -= li;
		if(remaining > 0) {
			r.next_node = icao_apdu_parse(ptr, remaining, msg_type);
		}
		r.consumed = len;	// whole buffer consumed
	} else {
// consume TPDU header only; next TPDU may be present
		r.consumed = 1 + li;
	}
	pdu->err = false;
	return r;
fail:
	r.next_node = unknown_proto_pdu_new(buf, len);
	return r;
}

la_proto_node *cotp_concatenated_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	GSList *pdu_list = NULL;
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_cotp_concatenated_pdu;
	node->next = NULL;

	while(len > 0) {
// Concatenated PDU is, as the name says, several COTP PDUs concatenated together.
// We therefore construct a GSList of cotp_pdu_t's. Only the last (final) PDU may
// contain higher level protocol described by its own la_type_descriptor,
// so we may simplify things a bit and provide only a single next node for the whole
// concatenated PDU instead of having a separate next node for each contained PDU,
// which (the next node) would be NULL anyway, except for the last one.
		debug_print("Remaining length: %u\n", len);
		cotp_pdu_parse_result r = cotp_pdu_parse(buf, len, msg_type);
		pdu_list = g_slist_append(pdu_list, r.pdu);
		if(r.next_node != NULL) {
// We reached final PDU and we have a next protocol node in the hierarchy.
			node->next = r.next_node;
		}
		if(r.pdu->err == true) {	// parsing failed
			break;
		}
		buf += r.consumed; len -= r.consumed;
	}
	node->data = pdu_list;
	return node;
}

static const dict cotp_tpdu_codes[] = {
	{ COTP_TPDU_CR, "Connect Request" },
	{ COTP_TPDU_CC, "Connect Confirm" },
	{ COTP_TPDU_DR, "Disconnect Request" },
	{ COTP_TPDU_DC, "Disconnect Confirm" },
	{ COTP_TPDU_DT, "Data" },
	{ COTP_TPDU_ED, "Expedited Data" },
	{ COTP_TPDU_AK, "Data Acknowledgement" },
	{ COTP_TPDU_EA, "Expedited Data Acknowledgement" },
	{ COTP_TPDU_RJ, "Reject" },
	{ COTP_TPDU_ER, "Error" },
	{ 0,  NULL }
};

static const dict cotp_dr_reasons[] = {
	{   0,	"Reason not specified" },
	{   1,	"TSAP congestion" },
	{   2,	"Session entity not attached to TSAP" },
	{   3,	"Unknown address" },
	{ 128,	"Normal disconnect" },
	{ 129,  "Remote transport entity congestion" },
	{ 130,	"Connection negotiation failed" },
	{ 131,	"Duplicate source reference" },
	{ 132,	"Mismatched references" },
	{ 133,	"Protocol error" },
	{ 135,	"Reference overflow" },
	{ 136,	"Connection request refused" },
	{ 138,	"Header or parameter length invalid" },
	{   0,  NULL }
};

static const dict cotp_er_reject_causes[] = {
	{ 0,	"Reason not specified" },
	{ 1,	"Invalid parameter code" },
	{ 2,	"Invalid TPDU type" },
	{ 3,	"Invalid parameter value" },
	{ 0,  NULL }
};

typedef struct {
	la_vstring *vstr;
	int indent;
} fmt_ctx_t;

static void output_cotp_pdu_as_text(gpointer p, gpointer user_data) {
	ASSERT(p != NULL);
	ASSERT(user_data != NULL);
	CAST_PTR(pdu, cotp_pdu_t *, p);
	CAST_PTR(ctx, fmt_ctx_t *, user_data);

	la_vstring *vstr = ctx->vstr;
	int indent = ctx->indent;
	char *str;

	if(pdu->err == true) {
		LA_ISPRINTF(vstr, indent, "%s", "-- Unparseable COTP TPDU\n");
		return;
	}
	char *tpdu_name = (char *)dict_search(cotp_tpdu_codes, pdu->code);
	ASSERT(tpdu_name != NULL);
	LA_ISPRINTF(vstr, indent, "COTP %s:\n", tpdu_name);
	indent++;
	switch(pdu->code) {
	case COTP_TPDU_CR:
	case COTP_TPDU_CC:
		LA_ISPRINTF(vstr, indent, "Protocol class: %u\n", pdu->class_or_status);
		break;
	case COTP_TPDU_ER:
		str = (char *)dict_search(cotp_er_reject_causes, pdu->class_or_status);
		LA_ISPRINTF(vstr, indent, "Reject cause: %u (%s)\n", pdu->class_or_status,
			(str ? str : "<unknown>"));
		break;
	case COTP_TPDU_DR:
		str = (char *)dict_search(cotp_dr_reasons, pdu->class_or_status);
		LA_ISPRINTF(vstr, indent, "Reason: %u (%s)\n", pdu->class_or_status,
			(str ? str : "<unknown>"));
		break;
	}
}

void cotp_concatenated_pdu_format_text(la_vstring * const vstr, void const * const data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	CAST_PTR(pdu_list, GSList *, data);
	g_slist_foreach(pdu_list, output_cotp_pdu_as_text, &(fmt_ctx_t){ .vstr = vstr, .indent = indent});
}

static void cotp_pdu_destroy(gpointer pdu) {
	XFREE(pdu);
}


void cotp_concatenated_pdu_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	CAST_PTR(pdu_list, GSList *, data);
	g_slist_free_full(pdu_list, cotp_pdu_destroy);
// No XFREE(data) here - g_slist_free_full frees the top pointer.
}

la_type_descriptor const proto_DEF_cotp_concatenated_pdu = {
	.format_text = cotp_concatenated_pdu_format_text,
	.destroy = cotp_concatenated_pdu_destroy
};
