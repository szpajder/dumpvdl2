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
#include <stdio.h>
#include <stdint.h>
#include <string.h>                 // strdup()
#include <libacars/libacars.h>      // la_proto_node
#include <libacars/vstring.h>       // la_vstring
#include <libacars/dict.h>          // la_dict
#include <libacars/json.h>
#include "dumpvdl2.h"
#include "tlv.h"
#include "clnp.h"
#include "reassembly.h"
#include "esis.h"                   // esis_pdu_parse()
#include "idrp.h"                   // idrp_pdu_parse()
#include "cotp.h"                   // cotp_concatenated_pdu_parse()
#include "atn.h"                    // atn_sec_label_parse, atn_sec_label_format_{text,json}

/***************************************************************************
 * Packet reassembly types and callbacks
 **************************************************************************/

// Can't use CLNP header as msg_info during reassembly, because when the header
// is compressed, it does not contain enough addressing information to
// discriminate packes uniquely. We need AVLC addresses to do this.  This
// structure is also used as a hash key in reassembly table.

struct clnp_reasm_key {
	uint32_t src_addr, dst_addr;
	uint16_t pdu_id;
};

void *clnp_reasm_key_get(void const *msg) {
	struct clnp_reasm_key const *key = msg;
	NEW(struct clnp_reasm_key, newkey);
	newkey->src_addr = key->src_addr;
	newkey->dst_addr = key->dst_addr;
	newkey->pdu_id = key->pdu_id;
	return newkey;
}

void clnp_reasm_key_destroy(void *ptr) {
	XFREE(ptr);
}

uint32_t clnp_reasm_key_hash(void const *ptr) {
	struct clnp_reasm_key const *key = ptr;
	uint32_t ret = key->src_addr * 11 + key->dst_addr * 23 + key->pdu_id * 31;
	return ret;
}

bool clnp_reasm_key_compare(void const *key1, void const *key2) {
	struct clnp_reasm_key const *k1 = key1;
	struct clnp_reasm_key const *k2 = key2;
	bool ret = k1->src_addr == k2->src_addr &&
		k1->dst_addr == k2->dst_addr &&
		k1->pdu_id == k2->pdu_id;
	return ret;
}

static la_reasm_table_funcs clnp_reasm_funcs = {
	.get_key = clnp_reasm_key_get,
	.get_tmp_key = clnp_reasm_key_get,
	.hash_key = clnp_reasm_key_hash,
	.compare_keys = clnp_reasm_key_compare,
	.destroy_key = clnp_reasm_key_destroy
};

#define CLNP_REASM_TABLE_CLEANUP_INTERVAL 20

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
			debug_print(D_PROTO, "CLNP inside CLNP? Bailing out to avoid loop\n");
			break;
		default:
			// assume X.224 COTP TPDU
			return cotp_concatenated_pdu_parse(buf, len, msg_type);
	}
	return unknown_proto_pdu_new(buf, len);
}

/**********************************
 * Uncompressed CLNP NPDU decoder
 **********************************/

// Forward declarations
la_type_descriptor const proto_DEF_clnp_pdu;
TLV_PARSER(clnp_error_code_parse);
TLV_PARSER(clnp_security_parse);
TLV_FORMATTER(clnp_error_code_format_text);
TLV_FORMATTER(clnp_error_code_format_json);

static la_dict const clnp_options[] = {
	// Doc 9705, 5.7.6.3.2.4.10
	{
		.id = 0x05,
		.val = &(tlv_type_descriptor_t){
			.label = "LRef",
			.json_key = "lref",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_single_octet_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		}
	},
	// Standard X.233 options
	{
		.id = 0xc3,
		.val = &(tlv_type_descriptor_t){
			.label = "QoS maintenance",
			.json_key = "qos_maintenance",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_single_octet_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0xc1,
		.val = &(tlv_type_descriptor_t){
			.label = "Discard reason",
			.json_key = "discard_reason",
			.parse = clnp_error_code_parse,
			.format_text = clnp_error_code_format_text,
			.format_json = clnp_error_code_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0xc4,
		.val = &(tlv_type_descriptor_t){
			.label = "Prefix-based scope control",
			.json_key = "prefix_based_scope_control",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0xc5,
		.val = &(tlv_type_descriptor_t){
			.label = "Security",
			.json_key = "security",
			.parse = clnp_security_parse,
			.format_text = atn_sec_label_format_text,
			.format_json = atn_sec_label_format_json,
			.destroy = atn_sec_label_destroy
		},
	},
	{
		.id = 0xc6,
		.val = &(tlv_type_descriptor_t){
			.label = "Radius scope control",
			.json_key = "radius_scope_control",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0xc8,
		.val = &(tlv_type_descriptor_t){
			.label = "Source routing",
			.json_key = "source_routing",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0xcb,
		.val = &(tlv_type_descriptor_t){
			.label = "Record route",
			.json_key = "record_route",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0xcc,
		.val = &(tlv_type_descriptor_t){
			.label = "Padding",
			.json_key = "padding",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0xcd,
		.val = &(tlv_type_descriptor_t){
			.label = "Priority",
			.json_key = "priority",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_single_octet_format_text,
			.format_json = tlv_octet_string_format_json,
			.destroy = NULL
		},
	},
	{
		.id = 0x0,
		.val = NULL
	}
};

la_proto_node *clnp_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	NEW(clnp_pdu_t, pdu);
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_clnp_pdu;
	node->data = pdu;
	node->next = NULL;

	pdu->err = true;    // fail-safe default
	uint8_t *ptr = buf;
	uint32_t remaining = len;
	if(remaining < CLNP_MIN_LEN) {
		debug_print(D_PROTO, "Too short (len %u < min len %u)\n", remaining, CLNP_MIN_LEN);
		goto fail;
	}

	clnp_hdr_t *hdr = (clnp_hdr_t *)ptr;
	pdu->hdr = hdr;
	if(hdr->len == 255) {
		debug_print(D_PROTO, "invalid length indicator - value 255 is reserved\n");
		goto fail;
	}
	if(remaining < hdr->len) {
		debug_print(D_PROTO, "header truncated: buf_len %u < len_indicator %u\n", remaining, hdr->len);
		goto fail;
	}
	if(hdr->version != 1) {
		debug_print(D_PROTO, "unsupported PDU version %u\n", hdr->version);
		goto fail;
	}
	pdu->lifetime_sec = (float)hdr->lifetime * 0.5f;
	pdu->seg_len = extract_uint16_msbfirst(hdr->seg_len);
	pdu->cksum = extract_uint16_msbfirst(hdr->cksum);
	debug_print(D_PROTO_DETAIL, "seg_len: %u, cksum: 0x%x\n", pdu->seg_len, pdu->cksum);
	ptr += sizeof(clnp_hdr_t); remaining -= sizeof(clnp_hdr_t);

	int ret = octet_string_parse(ptr, remaining, &(pdu->dst_nsap));
	if(ret < 0) {
		debug_print(D_PROTO, "failed to parse dst NET addr\n");
		goto fail;
	}
	ptr += ret; remaining -= ret;
	debug_print(D_PROTO_DETAIL, "dst NET: consumed %d octets, remaining: %u\n", ret, remaining);

	ret = octet_string_parse(ptr, remaining, &(pdu->src_nsap));
	if(ret < 0) {
		debug_print(D_PROTO, "failed to parse src NET addr\n");
		goto fail;
	}
	ptr += ret; remaining -= ret;
	debug_print(D_PROTO_DETAIL, "src NET: consumed %d octets, remaining: %u\n", ret, remaining);

	if(hdr->sp != 0) {  // segmentation part is present
		if(remaining < 6) {
			debug_print(D_PROTO, "segmentation part truncated: len %u < required 6\n", remaining);
			goto fail;
		}
		pdu->pdu_id = extract_uint16_msbfirst(ptr);
		pdu->seg_off = extract_uint16_msbfirst(ptr + 2);
		pdu->total_pdu_len = extract_uint16_msbfirst(ptr + 4);
		ptr += 6; remaining -= 6;
	}

	int options_part_len = hdr->len - (ptr - buf);
	debug_print(D_PROTO_DETAIL, "options_part_len: %d\n", options_part_len);
	if(options_part_len > 0) {
		pdu->options = tlv_parse(ptr, (size_t)options_part_len, clnp_options, 1);
		if(pdu->options == NULL) {
			debug_print(D_PROTO, "tlv_parse failed on options part\n");
			goto fail;
		}
	}

	// If this is an Error Report NPDU, the data part contains a header (and possibly some data)
	// of the NPDU which caused the error, so we re-run CLNP parser here.
	if(pdu->hdr->type == CLNP_NDPU_ER) {
		node->next = clnp_pdu_parse(buf + hdr->len, len - hdr->len, msg_type);
	} else {
		// Otherwise process as a normal CLNP payload.
		node->next = parse_clnp_pdu_payload(buf + hdr->len, len - hdr->len, msg_type);
	}
	pdu->err = false;
	return node;
fail:
	node->next = unknown_proto_pdu_new(buf, len);
	return node;
}

typedef struct {
	uint8_t code;
	uint8_t erroneous_octet;
} clnp_error_t;

TLV_PARSER(clnp_error_code_parse) {
	UNUSED(typecode);
	ASSERT(buf != NULL);

	if(len != 2) {
		return NULL;
	}
	NEW(clnp_error_t, e);
	e->code = buf[0];
	e->erroneous_octet = buf[1];
	return e;
}

static la_dict const clnp_error_codes[] = {
	{ .id = 0x00, .val = "Reason not specified" },
	{ .id = 0x01, .val = "Protocol procedure error" },
	{ .id = 0x02, .val = "Incorrect checksum" },
	{ .id = 0x03, .val = "PDU discarded due to congestion" },
	{ .id = 0x04, .val = "Header syntax error" },
	{ .id = 0x05, .val = "Segmentation needed but not permitted" },
	{ .id = 0x06, .val = "Incomplete PDU received" },
	{ .id = 0x07, .val = "Duplicate option" },
	{ .id = 0x08, .val = "Unknown PDU type" },
	{ .id = 0x80, .val = "Destination address unreachable" },
	{ .id = 0x81, .val = "Destination address unknown" },
	{ .id = 0x90, .val = "Unspecified source routing error" },
	{ .id = 0x91, .val = "Syntax error in source routing field" },
	{ .id = 0x92, .val = "Unknown address in source routing field" },
	{ .id = 0x93, .val = "Path not acceptable" },
	{ .id = 0xa0, .val = "Lifetime expired in transit" },
	{ .id = 0xa1, .val = "Lifetime expired during reassembly" },
	{ .id = 0xb0, .val = "Unsupported option" },
	{ .id = 0xb1, .val = "Unsupported protocol version" },
	{ .id = 0xb2, .val = "Unsupported security option" },
	{ .id = 0xb3, .val = "Unsupported source routing option" },
	{ .id = 0xb4, .val = "Unsupported record route option" },
	{ .id = 0xb5, .val = "Unsupported or unavailable QoS" },
	{ .id = 0xc0, .val = "Reassembly interference" },
	{ .id = 0, .val = NULL }
};

TLV_FORMATTER(clnp_error_code_format_text) {
	clnp_error_t const *e = data;
	char const *str = la_dict_search(clnp_error_codes, e->code);
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: %u (%s)", label, e->code, str ? str : "unknown");
	if(e->erroneous_octet != 0) {
		la_vstring_append_sprintf(ctx->vstr, ", erroneous octet value: 0x%02x", e->erroneous_octet);
	}
	EOL(ctx->vstr);
}

TLV_FORMATTER(clnp_error_code_format_json) {
	clnp_error_t const *e = data;
	la_json_object_start(ctx->vstr, label);
	la_json_append_int64(ctx->vstr, "error_code", e->code);
	char const *str = la_dict_search(clnp_error_codes, e->code);
	SAFE_JSON_APPEND_STRING(ctx->vstr, "error_descr", str);
	if(e->erroneous_octet != 0) {
		la_json_append_int64(ctx->vstr, "erroneous_octet", e->erroneous_octet);
	}
	la_json_object_end(ctx->vstr);
}

TLV_PARSER(clnp_security_parse) {
	ASSERT(buf != NULL);

	if(len < 1) {
		return NULL;
	}
	// The first octet contains security format code (X.233, 7.5.3).
	// In ATN its value is always 0xC0 (= globally unique security field).
	// ATN Security Label goes after that.
	return atn_sec_label_parse(typecode, buf + 1, len - 1);
}

static la_dict const clnp_pdu_types[] = {
	{ .id = CLNP_NPDU_DT, .val = "Data" },
	{ .id = CLNP_NDPU_MD, .val = "Multicast Data" },
	{ .id = CLNP_NDPU_ER, .val = "Error Report" },
	{ .id = CLNP_NDPU_ERP, .val = "Echo Request" },
	{ .id = CLNP_NDPU_ERQ, .val = "Echo Reply" },
	{ .id = 0, .val = NULL }
};

void clnp_pdu_format_text(la_vstring *vstr, void const *data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	clnp_pdu_t const *pdu = data;
	if(pdu->err == true) {
		LA_ISPRINTF(vstr, indent, "%s", "-- Unparseable X.233 CLNP PDU\n");
		return;
	}
	char const *pdu_type = la_dict_search(clnp_pdu_types, pdu->hdr->type);
	if(pdu_type != NULL) {
		LA_ISPRINTF(vstr, indent, "X.233 CLNP %s:\n", pdu_type);
	} else {
		LA_ISPRINTF(vstr, indent, "X.233 CLNP unknown PDU (code=0x%02x):\n", pdu->hdr->type);
	}
	indent++;

	LA_ISPRINTF(vstr, indent, "%s: ", "Src NSAP");
	octet_string_with_ascii_format_text(vstr, &pdu->src_nsap, 0);
	EOL(vstr);

	LA_ISPRINTF(vstr, indent, "%s: ", "Dst NSAP");
	octet_string_with_ascii_format_text(vstr, &pdu->dst_nsap, 0);
	EOL(vstr);

	LA_ISPRINTF(vstr, indent, "Lifetime: %.1f sec\n", pdu->lifetime_sec);
	LA_ISPRINTF(vstr, indent, "Flags:%s%s%s\n",
			pdu->hdr->sp ? " SP" : "",
			pdu->hdr->ms ? " MS" : "",
			pdu->hdr->er ? " E/R" : "");
	// LA_ISPRINTF(vstr, indent, "Segment length: %u\n", pdu->seg_len);
	// LA_ISPRINTF(vstr, indent, "Checksum: %x\n", pdu->cksum);

	if(pdu->hdr->sp != 0) {
		LA_ISPRINTF(vstr, indent, "%s", "Segmentation:\n");
		indent++;
		LA_ISPRINTF(vstr, indent, "PDU Id: 0x%x\n", pdu->pdu_id);
		LA_ISPRINTF(vstr, indent, "Segment offset: %u\n", pdu->seg_off);
		LA_ISPRINTF(vstr, indent, "PDU total length: %u\n", pdu->total_pdu_len);
		indent--;
	}
	if(pdu->options != NULL) {
		LA_ISPRINTF(vstr, indent, "%s", "Options:\n");
		tlv_list_format_text(vstr, pdu->options, indent+1);
	}
	if(pdu->hdr->type == CLNP_NDPU_ER) {
		LA_ISPRINTF(vstr, indent-1, "%s", "Erroneous NPDU:\n");
	}
}

void clnp_pdu_format_json(la_vstring * vstr, void const *data) {
	ASSERT(vstr != NULL);
	ASSERT(data);

	clnp_pdu_t const *pdu = data;
	la_json_append_bool(vstr, "err", pdu->err);
	if(pdu->err == true) {
		return;
	}
	la_json_append_bool(vstr, "compressed", false);
	la_json_append_int64(vstr, "pdu_type", pdu->hdr->type);
	char const *pdu_type = la_dict_search(clnp_pdu_types, pdu->hdr->type);
	SAFE_JSON_APPEND_STRING(vstr, "pdu_type_name", pdu_type);
	la_json_append_octet_string(vstr, "src_nsap", pdu->src_nsap.buf, pdu->src_nsap.len);
	la_json_append_octet_string(vstr, "dst_nsap", pdu->dst_nsap.buf, pdu->dst_nsap.len);
	la_json_append_double(vstr, "lifetime", pdu->lifetime_sec);

	la_json_object_start(vstr, "flags");
	la_json_append_bool(vstr, "SP", pdu->hdr->sp);
	la_json_append_bool(vstr, "MS", pdu->hdr->ms);
	la_json_append_bool(vstr, "ER", pdu->hdr->er);
	la_json_object_end(vstr);

	if(pdu->hdr->sp != 0) {
	    la_json_object_start(vstr, "segmentation");
	    la_json_append_int64(vstr, "pdu_id", pdu->pdu_id);
	    la_json_append_int64(vstr, "segment_offset", pdu->seg_off);
	    la_json_append_int64(vstr, "pdu_total_len", pdu->total_pdu_len);
	    la_json_object_end(vstr);
	}

	if(pdu->options != NULL) {
		tlv_list_format_json(vstr, "options", pdu->options);
	}
}

void clnp_pdu_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	clnp_pdu_t *pdu = data;
	tlv_list_destroy(pdu->options);
	pdu->options = NULL;
	XFREE(data);
}

la_type_descriptor const proto_DEF_clnp_pdu = {
	.format_text = clnp_pdu_format_text,
	.format_json = clnp_pdu_format_json,
	.json_key = "clnp",
	.destroy = clnp_pdu_destroy,
};

/**********************************
 * Compressed CLNP NPDU decoder
 **********************************/

// Forward declaration
la_type_descriptor const proto_DEF_clnp_compressed_data_pdu;

la_proto_node *clnp_compressed_data_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type,
		la_reasm_ctx *rtables, struct timeval rx_time, uint32_t src_addr, uint32_t dst_addr) {
	NEW(clnp_compressed_data_pdu_t, pdu);
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_clnp_compressed_data_pdu;
	node->data = pdu;
	node->next = NULL;

	pdu->err = true;    // fail-safe default
	if(len < CLNP_COMPRESSED_MIN_LEN) {
		debug_print(D_PROTO, "Too short (len %u < min len %u)\n", len, CLNP_COMPRESSED_MIN_LEN);
		goto fail;
	}

	uint32_t hdrlen = CLNP_COMPRESSED_MIN_LEN;
	uint8_t *ptr = buf;
	uint32_t remaining = len;
	clnp_compressed_data_pdu_hdr_t *hdr = (clnp_compressed_data_pdu_hdr_t *)ptr;
	pdu->hdr = hdr;
	pdu->derived =
		hdr->type == 0x6 ||
		hdr->type == 0x7 ||
		hdr->type == 0x9 ||
		hdr->type == 0xa;
	if(hdr->exp != 0) {
		hdrlen += 1;  // EXP flag = 1 means localRef/B octet is present
	}
	pdu->is_segmentation_permitted =
		hdr->type == 0x1 || hdr->type == 0x3 || pdu->derived;
	if(pdu->is_segmentation_permitted) {
		hdrlen += 2;  // if SP flag is on, then PDU identifier is present
	}
	if(pdu->derived) {
		hdrlen += 4;  // If PDU is fragmented, then the offset and total length fields are present
	}
	pdu->more_segments = hdr->type == 0x7 || hdr->type == 0xa;
	// hdr->lifetime is expressed in half-seconds
	pdu->lifetime.tv_sec = hdr->lifetime >> 1;
	pdu->lifetime.tv_usec = 500000 * (hdr->lifetime & 1);

	debug_print(D_PROTO, "hdrlen: %u type: %02x prio: %02x lifetime: %02x flags: %02x exp: %d lref_a: %02x\n",
			hdrlen, hdr->type, hdr->priority, hdr->lifetime, hdr->flags.val, hdr->exp, hdr->lref_a);

	if(remaining < hdrlen) {
		debug_print(D_PROTO, "header truncated: buf_len %u < hdr_len %u\n", remaining, hdrlen);
		goto fail;
	}
	ptr += 4; remaining -= 4;
	if(hdr->exp != 0) {
		debug_print(D_PROTO_DETAIL, "lref_b: %02x\n", ptr[0]);
		pdu->lref = ((uint16_t)(hdr->lref_a) << 8) | (uint16_t)ptr[0];
		ptr++; remaining--;
	} else {
		pdu->lref = (uint16_t)(hdr->lref_a);
	}
	if(pdu->is_segmentation_permitted) {
		pdu->pdu_id = extract_uint16_msbfirst(ptr);
		ptr += 2; remaining -= 2;
	}
	if(pdu->derived) {
		pdu->offset = extract_uint16_msbfirst(ptr);
		uint32_t total_pdu_len = extract_uint16_msbfirst(ptr + 2);
		// Rudimentary check if the PDU makes sense, ie. whether the offset + length does
		// not exceed the value of total PDU length field. It might be just an incomplete
		// X.25 reassembly result, which resembles a CLNP derived PDU.
		ptr += 4; remaining -= 4;
		if(pdu->offset + remaining > total_pdu_len) {
			debug_print(D_PROTO, "offset %hu + fragment data length %u > total_pdu_len %u. "
					"Probably it's not a CLNP derived PDU.\n", pdu->offset, remaining, total_pdu_len);
			goto fail;
		}
	}

	bool decode_payload = true;
	if(pdu->derived && rtables != NULL) {   // reassembly engine is enabled
		decode_payload = false;
		la_reasm_table *clnp_rtable = la_reasm_table_lookup(rtables, &proto_DEF_clnp_compressed_data_pdu);
		if(clnp_rtable == NULL) {
			clnp_rtable = la_reasm_table_new(rtables, &proto_DEF_clnp_compressed_data_pdu,
					clnp_reasm_funcs, CLNP_REASM_TABLE_CLEANUP_INTERVAL);
		}
		struct clnp_reasm_key reasm_key = {
			.src_addr = src_addr, .dst_addr = dst_addr, .pdu_id = pdu->pdu_id
		};
		pdu->rstatus = reasm_fragment_add(clnp_rtable,
				&(reasm_fragment_info){
				.pdu_info = &reasm_key,
				.fragment_data = ptr,
				.fragment_data_len = remaining,
				.rx_time = rx_time,
				.reasm_timeout = pdu->lifetime,
				.offset = pdu->offset,
				.is_final_fragment = !pdu->more_segments,
				});
		debug_print(D_MISC, "PDU %d: rstatus: %s\n", pdu->pdu_id, reasm_status_name_get(pdu->rstatus));
		int reassembled_len = 0;
		if(pdu->rstatus == REASM_COMPLETE &&
				(reassembled_len = reasm_payload_get(clnp_rtable, &reasm_key, &ptr)) > 0) {
			remaining = reassembled_len;
			// ptr now points onto a newly allocated buffer.
			// Keep the pointer for freeing it later.
			pdu->reasm_buf = ptr;
			decode_payload = true;
		} else if(pdu->rstatus == REASM_SKIPPED) {
			decode_payload = true;
		}
	}
	node->next = decode_payload == true ?
		parse_clnp_pdu_payload(ptr, remaining, msg_type) :
		unknown_proto_pdu_new(ptr, remaining);

	pdu->err = false;
	return node;
fail:
	node->next = unknown_proto_pdu_new(buf, len);
	return node;
}

void clnp_compressed_data_pdu_format_text(la_vstring *vstr, void const *data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	clnp_compressed_data_pdu_t const *pdu = data;
	if(pdu->err == true) {
		LA_ISPRINTF(vstr, indent, "%s", "-- Unparseable X.233 CLNP compressed header PDU\n");
		return;
	}
	LA_ISPRINTF(vstr, indent, "%s", "X.233 CLNP Data (compressed header):\n");
	indent++;
	LA_ISPRINTF(vstr, indent, "LRef: 0x%x Prio: %u Lifetime: %u Flags: 0x%02x\n",
			pdu->lref, pdu->hdr->priority, pdu->hdr->lifetime, pdu->hdr->flags.val);
	if(pdu->is_segmentation_permitted) {
		LA_ISPRINTF(vstr, indent, "PDU Id: %u\n", pdu->pdu_id);
	}
	if(pdu->derived) {
		LA_ISPRINTF(vstr, indent, "Offset: %hu More: %d\n",
				pdu->offset, pdu->more_segments);
		LA_ISPRINTF(vstr, indent, "CLNP reasm status: %s\n",
				reasm_status_name_get(pdu->rstatus));
	}
}

void clnp_compressed_data_pdu_format_json(la_vstring *vstr, void const *data) {
	ASSERT(vstr != NULL);
	ASSERT(data);

	clnp_compressed_data_pdu_t const *pdu = data;
	la_json_append_bool(vstr, "err", pdu->err);
	if(pdu->err == true) {
		return;
	}
	la_json_append_bool(vstr, "compressed", true);
	la_json_append_int64(vstr, "local_ref_a", pdu->lref);
	la_json_append_int64(vstr, "priority", pdu->hdr->priority);
	la_json_append_int64(vstr, "lifetime", pdu->hdr->lifetime);
	la_json_append_int64(vstr, "flags", pdu->hdr->flags.val);
	if(pdu->is_segmentation_permitted) {
		la_json_append_int64(vstr, "pdu_id", pdu->pdu_id);
	}
	if(pdu->derived) {
		la_json_append_int64(vstr, "offset", pdu->offset);
		la_json_append_bool(vstr, "more", pdu->more_segments);
		la_json_append_string(vstr, "reasm_status", reasm_status_name_get(pdu->rstatus));
	}
}

la_type_descriptor const proto_DEF_clnp_compressed_data_pdu = {
	.format_text = clnp_compressed_data_pdu_format_text,
	.format_json = clnp_compressed_data_pdu_format_json,
	.json_key = "clnp",
	.destroy = NULL,
};
