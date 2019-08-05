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
#include <glib.h>
#include <libacars/libacars.h>		// la_proto_node
#include <libacars/vstring.h>		// la_vstring
#include "dumpvdl2.h"
#include "tlv.h"
#include "cotp.h"
#include "icao.h"

// X.225 Session Protocol Machine disconnect reason codes
#define SPM_PROTOCOL_ERROR 0
#define SPM_DISC_NORMAL_NO_REUSE 1
#define SPM_DISC_NORMAL_REUSE_NOT_POSSIBLE 2
#define SPM_DISC_REASON_MAX SPM_DISC_NORMAL_REUSE_NOT_POSSIBLE

// Forward declarations
TLV_PARSER(tpdu_size_parse);
TLV_PARSER(flow_control_confirmation_parse);
TLV_FORMATTER(flow_control_confirmation_format_text);
la_type_descriptor const proto_DEF_cotp_concatenated_pdu;

// Some rarely used parameters which are not required to be supported
// in the ATN are printed as hex strings. There's no point in providing
// specific formatting routines for them, since they will probably never
// be used in practice.
static dict const cotp_variable_part_params[] = {
	{
		.id = 0x08,
		.val = &(tlv_type_descriptor_t){
			.label = "ATN checksum",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0x85,
		.val = &(tlv_type_descriptor_t){
			.label = "Ack time (ms)",
			.parse = tlv_uint16_msbfirst_parse,
			.format_text = tlv_uint_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0x86,		// not required
		.val = &(tlv_type_descriptor_t){
			.label = "Residual error rate",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0x87,
		.val = &(tlv_type_descriptor_t){
			.label = "Priority",
			.parse = tlv_uint16_msbfirst_parse,
			.format_text = tlv_uint_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0x88,		// not required
		.val = &(tlv_type_descriptor_t){
			.label = "Transit delay",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0x89,		// not required
		.val = &(tlv_type_descriptor_t){
			.label = "Throughput",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0x8a,
		.val = &(tlv_type_descriptor_t){
			.label = "Subsequence number",
			.parse = tlv_uint16_msbfirst_parse,
			.format_text = tlv_uint_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0x8b,
		.val = &(tlv_type_descriptor_t){
			.label = "Reassignment time (s)",
			.parse = tlv_uint16_msbfirst_parse,
			.format_text = tlv_uint_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0x8c,
		.val = &(tlv_type_descriptor_t){
			.label = "Flow control",
			.parse = flow_control_confirmation_parse,
			.format_text = flow_control_confirmation_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0x8f,
		.val = &(tlv_type_descriptor_t){
			.label = "Selective ACK",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0xc0,
		.val = &(tlv_type_descriptor_t){
			.label = "TPDU size (bytes)",
			.parse = tpdu_size_parse,
			.format_text = tlv_uint_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0xc1,
		.val = &(tlv_type_descriptor_t){
			.label = "Calling transport selector",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0xc2,
		.val = &(tlv_type_descriptor_t){
			.label = "Called/responding transport selector",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0xc3,
		.val = &(tlv_type_descriptor_t){
			.label = "Checksum",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0xc4,
		.val = &(tlv_type_descriptor_t){
			.label = "Version",
			.parse = tlv_uint8_parse,
			.format_text = tlv_uint_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0xc5,		// not required
		.val = &(tlv_type_descriptor_t){
			.label = "Protection params",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0xc6,
		.val = &(tlv_type_descriptor_t){
			.label = "Additional options",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0xc7,
		.val = &(tlv_type_descriptor_t){
			.label = "Additional protocol class(es)",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0xe0,		// DR
		.val = &(tlv_type_descriptor_t){
			.label = "Additional info",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0xf0,		// not required
		.val = &(tlv_type_descriptor_t){
			.label = "Preferred max. TPDU size (bytes)",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0xf2,
		.val = &(tlv_type_descriptor_t){
			.label = "Inactivity timer (ms)",
			.parse = tlv_uint32_msbfirst_parse,
			.format_text = tlv_uint_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0x00,
		.val = NULL
	}
};

// Can't use cotp_variable_part_params for ER, because parameter 0xc1
// has a different meaning.
static dict const cotp_er_variable_part_params[] = {
	{
		.id = 0xc1,
		.val = &(tlv_type_descriptor_t){
			.label = "Invalid TPDU header",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 0xc3,
		.val = &(tlv_type_descriptor_t){
			.label = "Checksum",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 0x00,
		.val = NULL
	}
};

TLV_PARSER(tpdu_size_parse) {
	UNUSED(typecode);
	if(len != 1) return NULL;
	if(buf[0] < 0x7 || buf[0] >> 0xd) return NULL;
	NEW(uint32_t, ret);
	*ret = 1 << buf[0];
	return ret;
}

typedef struct {
	uint32_t acked_tpdu_nr;
	uint16_t acked_subseq;
	uint16_t acked_credit;
} cotp_flow_control_confirm_t;

TLV_PARSER(flow_control_confirmation_parse) {
	UNUSED(typecode);
	if(len != 8) return NULL;
	NEW(cotp_flow_control_confirm_t, ret);
	ret->acked_tpdu_nr = extract_uint32_msbfirst(buf) & 0x7fffffffu;
	ret->acked_subseq  = extract_uint16_msbfirst(buf + 4);
	ret->acked_credit  = extract_uint16_msbfirst(buf + 6);
	return ret;
}

TLV_FORMATTER(flow_control_confirmation_format_text) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);
	CAST_PTR(f, cotp_flow_control_confirm_t *, data);
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s:\n", label);
	LA_ISPRINTF(ctx->vstr, ctx->indent+1, "Acked TPDU nr: %u\n", f->acked_tpdu_nr);
	LA_ISPRINTF(ctx->vstr, ctx->indent+1, "Acked subsequence: %hu\n", f->acked_subseq);
	LA_ISPRINTF(ctx->vstr, ctx->indent+1, "Acked credit: %hu\n", f->acked_credit);
}

#define TPDU_CHECK_LEN(len, val, goto_on_fail) \
	do { \
		if((len) < (val)) { \
			debug_print("Truncated TPDU: len: %u < %u\n", (len), (val)); \
			goto goto_on_fail; \
		} \
	} while(0)

typedef struct {
	cotp_pdu_t *pdu;
	la_proto_node *next_node;
	int consumed;
} cotp_pdu_parse_result;

static cotp_pdu_parse_result cotp_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	cotp_pdu_parse_result r = { NULL, NULL, 0 };
	NEW(cotp_pdu_t, pdu);
	r.pdu = pdu;

	pdu->err = true;			// fail-safe default
	pdu->x225_xport_disc_reason = -1;	// X.225 xport disc reason not present
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
		pdu->credit = (uint16_t)(code & 0x0f);
		break;
	case COTP_TPDU_DT:
		pdu->code = code & 0xfe;
		pdu->roa = code & 0x1;
		break;
	default:
		pdu->code = code;
	}
	debug_print("TPDU code: 0x%02x\n", pdu->code);

	pdu->dst_ref = extract_uint16_msbfirst(ptr + 1);

	uint8_t variable_part_offset = 0;
	dict const *cotp_params = cotp_variable_part_params;
	switch(pdu->code) {
	case COTP_TPDU_CR:
	case COTP_TPDU_CC:
	case COTP_TPDU_DR:
		TPDU_CHECK_LEN(remaining, 6, fail);
		pdu->src_ref = extract_uint16_msbfirst(ptr + 3);

		if(pdu->code == COTP_TPDU_DR) {
			pdu->class_or_disc_reason = ptr[5];		// reason
		} else {						// CR or CC
			pdu->class_or_disc_reason = ptr[5] >> 4;	// protocol class
			pdu->options = ptr[5] & 0xf;
		}
		variable_part_offset = 6;
		final_pdu = 1;
		break;
	case COTP_TPDU_ER:
		TPDU_CHECK_LEN(remaining, 4, fail);
		pdu->class_or_disc_reason = ptr[3];			// reject cause
		variable_part_offset = 4;
		cotp_params = cotp_er_variable_part_params;
		break;
	case COTP_TPDU_DT:
	case COTP_TPDU_ED:
// If the header length is odd, assume it's an extended format.
// This assumption holds true only if the length of all options in the variable part
// is even (which is true for all options described in X.224 and Doc9705).
		if(li & 1) {
			TPDU_CHECK_LEN(remaining, 7, fail);
			pdu->eot = (ptr[3] & 0x80) >> 7;
			pdu->tpdu_seq = extract_uint32_msbfirst(ptr + 3) & 0x7fffffffu;
			variable_part_offset = 7;
			pdu->extended = true;
		} else {			// normal format
			TPDU_CHECK_LEN(remaining, 4, fail);
			pdu->eot = (ptr[3] & 0x80) >> 7;
			pdu->tpdu_seq = (uint32_t)(ptr[3] & 0x7f);
			variable_part_offset = 4;
			pdu->extended = false;
		}
		final_pdu = 1;
		break;
	case COTP_TPDU_DC:
		pdu->src_ref = extract_uint16_msbfirst(ptr + 3);
		variable_part_offset = 5;
		break;
	case COTP_TPDU_AK:
		if(li & 1) {
			TPDU_CHECK_LEN(remaining, 9, fail);
			pdu->tpdu_seq = extract_uint32_msbfirst(ptr + 3) & 0x7fffffffu;
			pdu->credit = extract_uint16_msbfirst(ptr + 7);
			variable_part_offset = 9;
			pdu->extended = true;
		} else {
			TPDU_CHECK_LEN(remaining, 4, fail);
			pdu->tpdu_seq = (uint32_t)(ptr[3] & 0x7f);
			variable_part_offset = 4;
			pdu->extended = false;
		}
		break;
	case COTP_TPDU_EA:
		if(li & 1) {
			TPDU_CHECK_LEN(remaining, 7, fail);
			pdu->tpdu_seq = extract_uint32_msbfirst(ptr + 3) & 0x7fffffffu;
			variable_part_offset = 7;
			pdu->extended = true;
		} else {
			TPDU_CHECK_LEN(remaining, 4, fail);
			pdu->tpdu_seq = (uint32_t)(ptr[3] & 0x7f);
			variable_part_offset = 4;
			pdu->extended = false;
		}
		break;
	case COTP_TPDU_RJ:
		if(li & 1) {
			TPDU_CHECK_LEN(remaining, 9, fail);
			pdu->tpdu_seq = extract_uint32_msbfirst(ptr + 3) & 0x7fffffffu;
			pdu->credit = extract_uint16_msbfirst(ptr + 7);
			pdu->extended = true;
		} else {
			TPDU_CHECK_LEN(remaining, 4, fail);
			pdu->tpdu_seq = (uint32_t)(ptr[3] & 0x7f);
			pdu->extended = false;
		}
		break;
	default:
		debug_print("Unknown TPDU code 0x%02x\n", pdu->code);
		goto fail;
	}
	if(variable_part_offset > 0 && remaining > variable_part_offset) {
		pdu->variable_part_params = tlv_parse(ptr + variable_part_offset,
			li - variable_part_offset, cotp_params, 1);
		if(pdu->variable_part_params == NULL) {
			debug_print("tlv_parse failed on variable part\n");
			goto fail;
		}
	}
	if(final_pdu) {
// user data is allowed in this PDU; if it's there, try to parse it
		ptr += li; remaining -= li;
		if(remaining > 0) {
			if(pdu->code == COTP_TPDU_DR && remaining == 1) {
// special case - single-byte user-data field in DR contains Session Protocol Machine
// disconnect reason code (X.225 6.6.4)
				if(ptr[0] <= SPM_DISC_REASON_MAX) {
					pdu->x225_xport_disc_reason = (int16_t)ptr[0];
				} else {
					r.next_node = unknown_proto_pdu_new(ptr, remaining);
				}
			} else {
				r.next_node = icao_apdu_parse(ptr, remaining, msg_type);
			}
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

static void output_cotp_pdu_as_text(gpointer p, gpointer user_data) {
	static char const * const x225_xport_disc_reason_codes[] = {
		[SPM_PROTOCOL_ERROR] = "Protocol error, cannnot sent ABORT SPDU",
		[SPM_DISC_NORMAL_NO_REUSE] = "OK, transport connection not reused",
		[SPM_DISC_NORMAL_REUSE_NOT_POSSIBLE] = "OK, transport connection reuse not possible"
	};

	static const dict cotp_tpdu_codes[] = {
		{ COTP_TPDU_CR, "Connect Request" },
		{ COTP_TPDU_CC, "Connect Confirm" },
		{ COTP_TPDU_DR, "Disconnect Request" },
		{ COTP_TPDU_DC, "Disconnect Confirm" },
		{ COTP_TPDU_DT, "Data" },
		{ COTP_TPDU_ED, "Expedited Data" },
		{ COTP_TPDU_AK, "Data Ack" },
		{ COTP_TPDU_EA, "Expedited Data Ack" },
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

	ASSERT(p != NULL);
	ASSERT(user_data != NULL);
	CAST_PTR(pdu, cotp_pdu_t *, p);
	CAST_PTR(ctx, tlv_formatter_ctx_t *, user_data);

	la_vstring *vstr = ctx->vstr;
	int indent = ctx->indent;
	char *str;

	if(pdu->err == true) {
		LA_ISPRINTF(vstr, indent, "%s", "-- Unparseable X.224 COTP TPDU\n");
		return;
	}
	CAST_PTR(tpdu_name, char *, dict_search(cotp_tpdu_codes, pdu->code));
	ASSERT(tpdu_name != NULL);

	LA_ISPRINTF(vstr, indent, "X.224 COTP %s%s:\n", tpdu_name,
		pdu->extended ? " (extended)" : "");
	indent++;

	switch(pdu->code) {
	case COTP_TPDU_CR:
	case COTP_TPDU_CC:
	case COTP_TPDU_DR:
	case COTP_TPDU_DC:
		LA_ISPRINTF(vstr, indent, "src_ref: 0x%04x dst_ref: 0x%04x\n", pdu->src_ref, pdu->dst_ref);
		break;
	default:
		LA_ISPRINTF(vstr, indent, "dst_ref: 0x%04x\n", pdu->dst_ref);
	}

	switch(pdu->code) {
	case COTP_TPDU_CR:
	case COTP_TPDU_CC:
		LA_ISPRINTF(vstr, indent, "Initial Credit: %hu\n", pdu->credit);
		LA_ISPRINTF(vstr, indent, "Protocol class: %u\n", pdu->class_or_disc_reason);
		LA_ISPRINTF(vstr, indent, "Options: %02x (use %s PDU formats)\n", pdu->options,
			pdu->options & 2 ? "extended" : "normal");
		break;
	case COTP_TPDU_AK:
	case COTP_TPDU_RJ:
		LA_ISPRINTF(vstr, indent, "rseq: %u credit: %hu\n", pdu->tpdu_seq, pdu->credit);
		break;
	case COTP_TPDU_EA:
		LA_ISPRINTF(vstr, indent, "rseq: %u\n", pdu->tpdu_seq);
		break;
	case COTP_TPDU_ER:
		str = (char *)dict_search(cotp_er_reject_causes, pdu->class_or_disc_reason);
		LA_ISPRINTF(vstr, indent, "Reject cause: %u (%s)\n", pdu->class_or_disc_reason,
			(str ? str : "<unknown>"));
		break;
	case COTP_TPDU_DT:
	case COTP_TPDU_ED:
		LA_ISPRINTF(vstr, indent, "sseq: %u req_of_ack: %u EoT: %u\n",
			pdu->tpdu_seq, pdu->roa, pdu->eot);
		break;
	case COTP_TPDU_DR:
		str = (char *)dict_search(cotp_dr_reasons, pdu->class_or_disc_reason);
		LA_ISPRINTF(vstr, indent, "Reason: %u (%s)\n", pdu->class_or_disc_reason,
			(str ? str : "<unknown>"));
		break;
	case COTP_TPDU_DC:
		break;
	}
	tlv_list_format_text(vstr, pdu->variable_part_params, indent);

	if(pdu->code == COTP_TPDU_DR && pdu->x225_xport_disc_reason >= 0) {
		LA_ISPRINTF(vstr, indent,
			"X.225 disconnect reason: %hd (%s)\n",
			pdu->x225_xport_disc_reason,
			x225_xport_disc_reason_codes[pdu->x225_xport_disc_reason]
		);
	}
}

void cotp_concatenated_pdu_format_text(la_vstring * const vstr, void const * const data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	CAST_PTR(pdu_list, GSList *, data);
	g_slist_foreach(pdu_list, output_cotp_pdu_as_text, &(tlv_formatter_ctx_t){ .vstr = vstr, .indent = indent});
}

static void cotp_pdu_destroy(gpointer ptr) {
	CAST_PTR(pdu, cotp_pdu_t *, ptr);
	tlv_list_destroy(pdu->variable_part_params);
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
