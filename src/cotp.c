/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2023 Tomasz Lemiech <szpajder@gmail.com>
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
#include <sys/time.h>               // struct timeval
#include <libacars/libacars.h>      // la_proto_node
#include <libacars/vstring.h>       // la_vstring
#include <libacars/dict.h>          // la_dict
#include <libacars/list.h>          // la_list
#include <libacars/json.h>
#include <libacars/reassembly.h>
#include "dumpvdl2.h"
#include "tlv.h"
#include "cotp.h"
#include "icao.h"

/***************************************************************************
 * Packet reassembly types and callbacks
 **************************************************************************/

#define COTP_REASM_TABLE_CLEANUP_INTERVAL 10
#define COTP_REASM_TIMEOUT_SECONDS 30

struct cotp_reasm_key {
	uint32_t src_addr, dst_addr;
	uint16_t dst_ref;
};

// Allocates COTP persistent key for a new hash entry.
// As there are no allocations performed for cotp_reasm_key structure members,
// it is used as a temporary key allocator as well.
void *cotp_key_get(void const *msg) {
	ASSERT(msg != NULL);
	struct cotp_reasm_key const *key = msg;
	NEW(struct cotp_reasm_key, newkey);
	newkey->src_addr = key->src_addr;
	newkey->dst_addr = key->dst_addr;
	newkey->dst_ref = key->dst_ref;
	return (void *)newkey;
}

void cotp_key_destroy(void *ptr) {
	XFREE(ptr);
}

uint32_t cotp_key_hash(void const *key) {
	struct cotp_reasm_key const *k = key;
	return k->src_addr * 11 + k->dst_addr * 23 + k->dst_ref * 31;
}

bool cotp_key_compare(void const *key1, void const *key2) {
	struct cotp_reasm_key const *k1 = key1;
	struct cotp_reasm_key const *k2 = key2;
	return k1->src_addr == k2->src_addr &&
		k1->dst_addr == k2->dst_addr &&
		k1->dst_ref == k2->dst_ref;
}

static la_reasm_table_funcs cotp_reasm_funcs = {
	.get_key = cotp_key_get,
	.get_tmp_key = cotp_key_get,
	.hash_key = cotp_key_hash,
	.compare_keys = cotp_key_compare,
	.destroy_key = cotp_key_destroy
};

static struct timeval cotp_reasm_timeout = {
	.tv_sec = COTP_REASM_TIMEOUT_SECONDS,
	.tv_usec = 0
};

/***************************************************************************
 * Option parsers and formatters
 **************************************************************************/

// X.225 Session Protocol Machine disconnect reason codes
#define SPM_PROTOCOL_ERROR 0
#define SPM_DISC_NORMAL_NO_REUSE 1
#define SPM_DISC_NORMAL_REUSE_NOT_POSSIBLE 2
#define SPM_DISC_REASON_MAX SPM_DISC_NORMAL_REUSE_NOT_POSSIBLE

// Forward declarations
TLV_PARSER(tpdu_size_parse);
TLV_PARSER(flow_control_confirmation_parse);
TLV_FORMATTER(flow_control_confirmation_format_text);
TLV_FORMATTER(flow_control_confirmation_format_json);
la_type_descriptor const proto_DEF_cotp_concatenated_pdu;

// Some rarely used parameters which are not required to be supported
// in the ATN are printed as hex strings. There's no point in providing
// specific formatting routines for them, since they will probably never
// be used in practice.
static la_dict const cotp_variable_part_params[] = {
	{
		.id = 0x08,
		.val = &(tlv_type_descriptor_t){
			.label = "ATN checksum",
			.json_key = "atn_checksum",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0x85,
		.val = &(tlv_type_descriptor_t){
			.label = "Ack time (ms)",
			.json_key = "ack_time_ms",
			.parse = tlv_uint16_msbfirst_parse,
			.format_text = tlv_uint_format_text,
			.format_json = tlv_uint_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0x86,     // not required
		.val = &(tlv_type_descriptor_t){
			.label = "Residual error rate",
			.json_key = "residual_error_rate",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0x87,
		.val = &(tlv_type_descriptor_t){
			.label = "Priority",
			.json_key = "priority",
			.parse = tlv_uint16_msbfirst_parse,
			.format_text = tlv_uint_format_text,
			.format_json = tlv_uint_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0x88,     // not required
		.val = &(tlv_type_descriptor_t){
			.label = "Transit delay",
			.json_key = "transit_delay",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0x89,     // not required
		.val = &(tlv_type_descriptor_t){
			.label = "Throughput",
			.json_key = "throughput",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0x8a,
		.val = &(tlv_type_descriptor_t){
			.label = "Subsequence number",
			.json_key = "subseq_num",
			.parse = tlv_uint16_msbfirst_parse,
			.format_text = tlv_uint_format_text,
			.format_json = tlv_uint_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0x8b,
		.val = &(tlv_type_descriptor_t){
			.label = "Reassignment time (s)",
			.json_key = "reassignment_time_sec",
			.parse = tlv_uint16_msbfirst_parse,
			.format_text = tlv_uint_format_text,
			.format_json = tlv_uint_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0x8c,
		.val = &(tlv_type_descriptor_t){
			.label = "Flow control",
			.json_key = "flow_control",
			.parse = flow_control_confirmation_parse,
			.format_text = flow_control_confirmation_format_text,
			.format_json = flow_control_confirmation_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0x8f,
		.val = &(tlv_type_descriptor_t){
			.label = "Selective ACK",
			.json_key = "sack",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0xc0,
		.val = &(tlv_type_descriptor_t){
			.label = "TPDU size (bytes)",
			.json_key = "tpdu_size",
			.parse = tpdu_size_parse,
			.format_text = tlv_uint_format_text,
			.format_json = tlv_uint_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0xc1,
		.val = &(tlv_type_descriptor_t){
			.label = "Calling transport selector",
			.json_key = "calling_transport_selector",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0xc2,
		.val = &(tlv_type_descriptor_t){
			.label = "Called/responding transport selector",
			.json_key = "called_responding_transport_selector",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0xc3,
		.val = &(tlv_type_descriptor_t){
			.label = "Checksum",
			.json_key = "checksum",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0xc4,
		.val = &(tlv_type_descriptor_t){
			.label = "Version",
			.json_key = "version",
			.parse = tlv_uint8_parse,
			.format_text = tlv_uint_format_text,
			.format_json = tlv_uint_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0xc5,     // not required
		.val = &(tlv_type_descriptor_t){
			.label = "Protection params",
			.json_key = "protection_params",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0xc6,
		.val = &(tlv_type_descriptor_t){
			.label = "Additional options",
			.json_key = "additional_options",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_single_octet_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0xc7,
		.val = &(tlv_type_descriptor_t){
			.label = "Additional protocol class(es)",
			.json_key = "additional_proto_classes",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0xe0,     // DR
		.val = &(tlv_type_descriptor_t){
			.label = "Additional info",
			.json_key = "additional_info",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0xf0,     // not required
		.val = &(tlv_type_descriptor_t){
			.label = "Preferred max. TPDU size (bytes)",
			.json_key = "preferred_max_tpdu_size",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0xf2,
		.val = &(tlv_type_descriptor_t){
			.label = "Inactivity timer (ms)",
			.json_key = "inactivity_timer_ms",
			.parse = tlv_uint32_msbfirst_parse,
			.format_text = tlv_uint_format_text,
			.format_json = tlv_uint_format_json,
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
static la_dict const cotp_er_variable_part_params[] = {
	{
		.id = 0xc1,
		.val = &(tlv_type_descriptor_t){
			.label = "Invalid TPDU header",
			.json_key = "invalid_tpdu_header",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	{
		.id = 0xc3,
		.val = &(tlv_type_descriptor_t){
			.label = "Checksum",
			.json_key = "checksum",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
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
	cotp_flow_control_confirm_t const *f = data;
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s:\n", label);
	LA_ISPRINTF(ctx->vstr, ctx->indent+1, "Acked TPDU nr: %u\n", f->acked_tpdu_nr);
	LA_ISPRINTF(ctx->vstr, ctx->indent+1, "Acked subsequence: %hu\n", f->acked_subseq);
	LA_ISPRINTF(ctx->vstr, ctx->indent+1, "Acked credit: %hu\n", f->acked_credit);
}

TLV_FORMATTER(flow_control_confirmation_format_json) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	cotp_flow_control_confirm_t const *f = data;
	la_json_object_start(ctx->vstr, label);
	la_json_append_int64(ctx->vstr, "acked_tpdu_nr", f->acked_tpdu_nr);
	la_json_append_int64(ctx->vstr, "acked_subseq", f->acked_subseq);
	la_json_append_int64(ctx->vstr, "acked_credit", f->acked_credit);
	la_json_object_end(ctx->vstr);
}

#define TPDU_HDR_CHECK_LEN(len, val, goto_on_fail) \
	do { \
		if((len) < (val)) { \
			debug_print(D_PROTO, "TPDU header too short: len: %u < %u\n", (len), (val)); \
			goto goto_on_fail; \
		} \
	} while(0)

typedef struct {
	cotp_pdu_t *pdu;
	la_proto_node *next_node;
	int consumed;
} cotp_pdu_parse_result;

static cotp_pdu_parse_result cotp_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type,
		la_reasm_ctx *rtables, struct timeval rx_time, uint32_t src_addr, uint32_t dst_addr) {
	ASSERT(buf != NULL);
	cotp_pdu_parse_result r = { NULL, NULL, 0 };
	NEW(cotp_pdu_t, pdu);
	r.pdu = pdu;

	pdu->err = true;                            // fail-safe default
	pdu->x225_transport_disc_reason = -1;       // X.225 transport disc reason not present
	bool final_pdu = false;
	uint8_t *ptr = buf;
	uint32_t remaining = len;
	TPDU_HDR_CHECK_LEN(remaining, 4, fail);     // need at least LI + TPDU code + dst_ref

	uint8_t li = ptr[0];
	ptr++; remaining--;
	if(li == 0 || li == 255) {
		debug_print(D_PROTO, "invalid header length indicator: %u\n", li);
		goto fail;
	}
	if(remaining < li) {
		debug_print(D_PROTO, "header truncated: len %u < li %u\n", remaining, li);
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
	debug_print(D_PROTO_DETAIL, "TPDU code: 0x%02x\n", pdu->code);

	pdu->dst_ref = extract_uint16_msbfirst(ptr + 1);

	uint8_t variable_part_offset = 0;
	la_dict const *cotp_params = cotp_variable_part_params;
	switch(pdu->code) {
		case COTP_TPDU_CR:
		case COTP_TPDU_CC:
		case COTP_TPDU_DR:
			variable_part_offset = 6;
			TPDU_HDR_CHECK_LEN(li, variable_part_offset, fail);
			pdu->src_ref = extract_uint16_msbfirst(ptr + 3);

			if(pdu->code == COTP_TPDU_DR) {
				pdu->class_or_disc_reason = ptr[5];         // reason
			} else {                                        // CR or CC
				pdu->class_or_disc_reason = ptr[5] >> 4;    // protocol class
				pdu->options = ptr[5] & 0xf;
			}
			final_pdu = true;
			break;
		case COTP_TPDU_ER:
			variable_part_offset = 4;
			TPDU_HDR_CHECK_LEN(li, variable_part_offset, fail);
			pdu->class_or_disc_reason = ptr[3];            // reject cause
			cotp_params = cotp_er_variable_part_params;
			break;
		case COTP_TPDU_DT:
		case COTP_TPDU_ED:
			// If the header length is odd, assume it's an extended format.
			// This assumption holds true only if the length of all options in the variable part
			// is even (which is true for all options described in X.224 and Doc9705).
			if(li & 1) {
				variable_part_offset = 7;
				TPDU_HDR_CHECK_LEN(li, variable_part_offset, fail);
				pdu->eot = (ptr[3] & 0x80) >> 7;
				pdu->tpdu_seq = extract_uint32_msbfirst(ptr + 3) & 0x7fffffffu;
				pdu->extended = true;
			} else {    // normal format
				variable_part_offset = 4;
				TPDU_HDR_CHECK_LEN(li, variable_part_offset, fail);
				pdu->eot = (ptr[3] & 0x80) >> 7;
				pdu->tpdu_seq = (uint32_t)(ptr[3] & 0x7f);
				pdu->extended = false;
			}
			final_pdu = true;
			break;
		case COTP_TPDU_DC:
			variable_part_offset = 5;
			TPDU_HDR_CHECK_LEN(li, variable_part_offset, fail);
			pdu->src_ref = extract_uint16_msbfirst(ptr + 3);
			break;
		case COTP_TPDU_AK:
			if(li & 1) {
				variable_part_offset = 9;
				TPDU_HDR_CHECK_LEN(li, variable_part_offset, fail);
				pdu->tpdu_seq = extract_uint32_msbfirst(ptr + 3) & 0x7fffffffu;
				pdu->credit = extract_uint16_msbfirst(ptr + 7);
				pdu->extended = true;
			} else {
				variable_part_offset = 4;
				TPDU_HDR_CHECK_LEN(li, variable_part_offset, fail);
				pdu->tpdu_seq = (uint32_t)(ptr[3] & 0x7f);
				pdu->extended = false;
			}
			break;
		case COTP_TPDU_EA:
			if(li & 1) {
				variable_part_offset = 7;
				TPDU_HDR_CHECK_LEN(li, variable_part_offset, fail);
				pdu->tpdu_seq = extract_uint32_msbfirst(ptr + 3) & 0x7fffffffu;
				pdu->extended = true;
			} else {
				variable_part_offset = 4;
				TPDU_HDR_CHECK_LEN(li, variable_part_offset, fail);
				pdu->tpdu_seq = (uint32_t)(ptr[3] & 0x7f);
				pdu->extended = false;
			}
			break;
		case COTP_TPDU_RJ:
			if(li & 1) {
				TPDU_HDR_CHECK_LEN(li, 9, fail);
				pdu->tpdu_seq = extract_uint32_msbfirst(ptr + 3) & 0x7fffffffu;
				pdu->credit = extract_uint16_msbfirst(ptr + 7);
				pdu->extended = true;
			} else {
				TPDU_HDR_CHECK_LEN(li, 4, fail);
				pdu->tpdu_seq = (uint32_t)(ptr[3] & 0x7f);
				pdu->extended = false;
			}
			break;
		default:
			debug_print(D_PROTO, "Unknown TPDU code 0x%02x\n", pdu->code);
			goto fail;
	}
	if(variable_part_offset > 0 && li > variable_part_offset) {
		pdu->variable_part_params = tlv_parse(ptr + variable_part_offset,
				li - variable_part_offset, cotp_params, 1);
		if(pdu->variable_part_params == NULL) {
			debug_print(D_PROTO, "tlv_parse failed on variable part\n");
			goto fail;
		}
	}
	if(final_pdu == true) {
		// user data is allowed in this PDU; if it's there, try to parse it
		ptr += li; remaining -= li;
		if(remaining > 0) {
			if(pdu->code == COTP_TPDU_DR && remaining == 1) {
				// special case - single-byte user-data field in DR contains Session Protocol Machine
				// disconnect reason code (X.225 6.6.4)
				if(ptr[0] <= SPM_DISC_REASON_MAX) {
					pdu->x225_transport_disc_reason = (int16_t)ptr[0];
				} else {
					r.next_node = unknown_proto_pdu_new(ptr, remaining);
				}
			} else {
				bool decode_payload = true;
				// perform reassembly if it's a data TPDU and reassembly engine is enabled
				if((pdu->code == COTP_TPDU_DT || pdu->code == COTP_TPDU_ED) && rtables != NULL) {
					la_reasm_table *cotp_rtable = la_reasm_table_lookup(rtables, &proto_DEF_cotp_concatenated_pdu);
					if(cotp_rtable == NULL) {
						cotp_rtable = la_reasm_table_new(rtables, &proto_DEF_cotp_concatenated_pdu,
								cotp_reasm_funcs, COTP_REASM_TABLE_CLEANUP_INTERVAL);
					}
					struct cotp_reasm_key reasm_key = {
						.src_addr = src_addr, .dst_addr = dst_addr, .dst_ref = pdu->dst_ref
					};
					pdu->reasm_status = la_reasm_fragment_add(cotp_rtable,
							&(la_reasm_fragment_info){
							.msg_info = &reasm_key,
							.msg_data = ptr,
							.msg_data_len = remaining,
							.total_pdu_len = 0,     // not used here
							.seq_num = pdu->tpdu_seq,
							.seq_num_first = SEQ_FIRST_NONE,
							.seq_num_wrap = pdu->extended ? 0x7fffffffu : 0x7fu,
							.is_final_fragment = pdu->eot != 0,
							.rx_time = rx_time,
							.reasm_timeout = cotp_reasm_timeout
							});
					int reassembled_len = 0;
					if(pdu->reasm_status == LA_REASM_COMPLETE &&
							(reassembled_len = la_reasm_payload_get(cotp_rtable, &reasm_key, &ptr)) > 0) {
						remaining = reassembled_len;
						decode_payload = true;
						// cotp_data is a newly allocated buffer; keep the pointer for freeing it later
						pdu->reasm_buf = ptr;
					} else if((pdu->reasm_status == LA_REASM_IN_PROGRESS ||
								pdu->reasm_status == LA_REASM_DUPLICATE) &&
							Config.decode_fragments == false) {
						decode_payload = false;
					}
				}
				r.next_node = decode_payload ? icao_apdu_parse(ptr, remaining, msg_type) :
					unknown_proto_pdu_new(ptr, remaining);
			}
		}
		r.consumed = len;   // whole buffer consumed
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

la_proto_node *cotp_concatenated_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type,
		la_reasm_ctx *rtables, struct timeval rx_time, uint32_t src_addr, uint32_t dst_addr) {
	la_list *pdu_list = NULL;
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_cotp_concatenated_pdu;
	node->next = NULL;

	while(len > 0) {
		// Concatenated PDU is, as the name says, several COTP PDUs concatenated together.
		// We therefore construct a la_list of cotp_pdu_t's. Only the last (final) PDU may
		// contain higher level protocol described by its own la_type_descriptor,
		// so we may simplify things a bit and provide only a single next node for the whole
		// concatenated PDU instead of having a separate next node for each contained PDU,
		// which (the next node) would be NULL anyway, except for the last one.
		debug_print(D_PROTO_DETAIL, "Remaining length: %u\n", len);
		cotp_pdu_parse_result r = cotp_pdu_parse(buf, len, msg_type, rtables, rx_time, src_addr, dst_addr);
		pdu_list = la_list_append(pdu_list, r.pdu);
		if(r.next_node != NULL) {
			// We reached final PDU and we have a next protocol node in the hierarchy.
			node->next = r.next_node;
		}
		if(r.pdu->err == true) {    // parsing failed
			break;
		}
		buf += r.consumed; len -= r.consumed;
	}
	node->data = pdu_list;
	return node;
}

static char const *x225_transport_disc_reason_codes[] = {
	[SPM_PROTOCOL_ERROR] = "Protocol error, cannnot sent ABORT SPDU",
	[SPM_DISC_NORMAL_NO_REUSE] = "OK, transport connection not reused",
	[SPM_DISC_NORMAL_REUSE_NOT_POSSIBLE] = "OK, transport connection reuse not possible"
};

static const la_dict cotp_tpdu_codes[] = {
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
	{ 0, NULL }
};

static la_dict const cotp_dr_reasons[] = {
	{   0, "Reason not specified" },
	{   1, "TSAP congestion" },
	{   2, "Session entity not attached to TSAP" },
	{   3, "Unknown address" },
	{ 128, "Normal disconnect" },
	{ 129, "Remote transport entity congestion" },
	{ 130, "Connection negotiation failed" },
	{ 131, "Duplicate source reference" },
	{ 132, "Mismatched references" },
	{ 133, "Protocol error" },
	{ 135, "Reference overflow" },
	{ 136, "Connection request refused" },
	{ 138, "Header or parameter length invalid" },
	{   0, NULL }
};

static la_dict const cotp_er_reject_causes[] = {
	{ 0, "Reason not specified" },
	{ 1, "Invalid parameter code" },
	{ 2, "Invalid TPDU type" },
	{ 3, "Invalid parameter value" },
	{ 0, NULL }
};

// Executed with la_list_foreach()
static void output_cotp_pdu_as_text(void const *data, void const *ctx_ptr) {
	ASSERT(data != NULL);
	ASSERT(ctx_ptr != NULL);
	cotp_pdu_t const *pdu = data;
	tlv_formatter_ctx_t const *ctx = ctx_ptr;

	la_vstring *vstr = ctx->vstr;
	int indent = ctx->indent;

	if(pdu->err == true) {
		LA_ISPRINTF(vstr, indent, "%s", "-- Unparseable X.224 COTP TPDU\n");
		return;
	}
	char const *tpdu_name = la_dict_search(cotp_tpdu_codes, pdu->code);
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

	char const *str = NULL;
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
			str = la_dict_search(cotp_er_reject_causes, pdu->class_or_disc_reason);
			LA_ISPRINTF(vstr, indent, "Reject cause: %u (%s)\n", pdu->class_or_disc_reason,
					(str ? str : "<unknown>"));
			break;
		case COTP_TPDU_DT:
		case COTP_TPDU_ED:
			LA_ISPRINTF(vstr, indent, "sseq: %u req_of_ack: %u EoT: %u\n",
					pdu->tpdu_seq, pdu->roa, pdu->eot);
			LA_ISPRINTF(vstr, indent, "COTP reasm status: %s\n",
					la_reasm_status_name_get(pdu->reasm_status));
			break;
		case COTP_TPDU_DR:
			str = la_dict_search(cotp_dr_reasons, pdu->class_or_disc_reason);
			LA_ISPRINTF(vstr, indent, "Reason: %u (%s)\n", pdu->class_or_disc_reason,
					(str ? str : "<unknown>"));
			break;
		case COTP_TPDU_DC:
			break;
	}
	tlv_list_format_text(vstr, pdu->variable_part_params, indent);

	if(pdu->code == COTP_TPDU_DR && pdu->x225_transport_disc_reason >= 0) {
		LA_ISPRINTF(vstr, indent,
				"X.225 disconnect reason: %hd (%s)\n",
				pdu->x225_transport_disc_reason,
				x225_transport_disc_reason_codes[pdu->x225_transport_disc_reason]
				);
	}
}

// Executed with la_list_foreach()
static void output_cotp_pdu_as_json(void const *data, void const *ctx_ptr) {
	ASSERT(data != NULL);
	ASSERT(ctx_ptr != NULL);
	cotp_pdu_t const *pdu = data;
	tlv_formatter_ctx_t const *ctx = ctx_ptr;

	la_vstring *vstr = ctx->vstr;
	char *str;

	la_json_object_start(vstr, NULL);
	la_json_append_bool(vstr, "err", pdu->err);
	if(pdu->err == true) {
		goto end;
	}

	la_json_append_int64(vstr, "tpdu_code", pdu->code);
	char const *tpdu_name = la_dict_search(cotp_tpdu_codes, pdu->code);
	ASSERT(tpdu_name != NULL);
	la_json_append_string(vstr, "tpdu_code_descr", tpdu_name);
	la_json_append_bool(vstr, "extended", pdu->extended);

	switch(pdu->code) {
		case COTP_TPDU_CR:
		case COTP_TPDU_CC:
		case COTP_TPDU_DR:
		case COTP_TPDU_DC:
			la_json_append_int64(vstr, "src_ref", pdu->src_ref);
			/* FALLTHROUGH */
		default:
			la_json_append_int64(vstr, "dst_ref", pdu->dst_ref);
	}

	switch(pdu->code) {
		case COTP_TPDU_CR:
		case COTP_TPDU_CC:
			la_json_append_int64(vstr, "credit", pdu->credit);
			la_json_append_int64(vstr, "proto_class", pdu->class_or_disc_reason);
			la_json_append_int64(vstr, "options", pdu->options);
			la_json_append_bool(vstr, "use_extended_pdu_formats", pdu->options & 2);
			break;
		case COTP_TPDU_AK:
		case COTP_TPDU_RJ:
			la_json_append_int64(vstr, "credit", pdu->credit);
			la_json_append_int64(vstr, "rseq", pdu->tpdu_seq);
			break;
		case COTP_TPDU_EA:
			la_json_append_int64(vstr, "rseq", pdu->tpdu_seq);
			break;
		case COTP_TPDU_ER:
			la_json_append_int64(vstr, "reject_code", pdu->class_or_disc_reason);
			str = la_dict_search(cotp_er_reject_causes, pdu->class_or_disc_reason);
			SAFE_JSON_APPEND_STRING(vstr, "reject_cause", str);
			break;
		case COTP_TPDU_DT:
		case COTP_TPDU_ED:
			la_json_append_int64(vstr, "sseq", pdu->tpdu_seq);
			la_json_append_int64(vstr, "req_of_ack", pdu->roa);
			la_json_append_int64(vstr, "eot", pdu->eot);
			la_json_append_string(vstr, "reasm_status", la_reasm_status_name_get(pdu->reasm_status));
			break;
		case COTP_TPDU_DR:
			la_json_append_int64(vstr, "disc_reason_code", pdu->class_or_disc_reason);
			str = la_dict_search(cotp_dr_reasons, pdu->class_or_disc_reason);
			SAFE_JSON_APPEND_STRING(vstr, "disc_reason", str);
			break;
		case COTP_TPDU_DC:
			break;
	}
	tlv_list_format_json(vstr, "variable_part_params", pdu->variable_part_params);

	if(pdu->code == COTP_TPDU_DR && pdu->x225_transport_disc_reason >= 0) {
		la_json_append_int64(vstr, "x225_spm_transport_disconnect_reason_code", pdu->x225_transport_disc_reason);
		la_json_append_string(vstr, "x225_spm_transport_disconnect_reason",
				x225_transport_disc_reason_codes[pdu->x225_transport_disc_reason]);
	}
end:
	la_json_object_end(vstr);
}

void cotp_concatenated_pdu_format_text(la_vstring *vstr, void const *data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	la_list const *pdu_list = data;
	la_list_foreach((la_list *)pdu_list, output_cotp_pdu_as_text,
			&(tlv_formatter_ctx_t){ .vstr = vstr, .indent = indent});
}

void cotp_concatenated_pdu_format_json(la_vstring *vstr, void const *data) {
	ASSERT(vstr != NULL);
	ASSERT(data);

	la_list const *pdu_list = data;
	la_json_array_start(vstr, "pdu_list");
	la_list_foreach((la_list *)pdu_list, output_cotp_pdu_as_json,
			&(tlv_formatter_ctx_t){ .vstr = vstr, .indent = 0});
	la_json_array_end(vstr);
}


static void cotp_pdu_destroy(void *ptr) {
	cotp_pdu_t *pdu = ptr;
	tlv_list_destroy(pdu->variable_part_params);
	XFREE(pdu->reasm_buf);
	XFREE(pdu);
}

void cotp_concatenated_pdu_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	la_list *pdu_list = data;
	la_list_free_full(pdu_list, cotp_pdu_destroy);
	// No XFREE(data) here - la_list_free_full frees the top pointer.
}

la_type_descriptor const proto_DEF_cotp_concatenated_pdu = {
	.format_text = cotp_concatenated_pdu_format_text,
	.format_json = cotp_concatenated_pdu_format_json,
	.json_key = "cotp",
	.destroy = cotp_concatenated_pdu_destroy
};
