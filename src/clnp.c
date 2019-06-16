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
#include <string.h>			// strdup()
#include <libacars/libacars.h>		// la_proto_node
#include <libacars/vstring.h>		// la_vstring
#include "dumpvdl2.h"
#include "tlv2.h"
#include "clnp.h"
#include "esis.h"			// esis_pdu_parse()
#include "idrp.h"			// idrp_pdu_parse()
#include "cotp.h"			// cotp_concatenated_pdu_parse()

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

/**********************************
 * Uncompressed CLNP NPDU decoder
 **********************************/

// Forward declarations
la_type_descriptor const proto_DEF_clnp_pdu;
TLV2_PARSER(clnp_error_code_parse);
TLV2_FORMATTER(clnp_error_code_format_text);

static dict const clnp_options[] = {
// Doc 9705, 5.7.6.3.2.4.10
	{
		.id = 0x05,
		.val = &(tlv2_type_descriptor_t){
			.label = "LRef",
			.parse = tlv2_octet_string_parse,
			.format_text = tlv2_octet_string_format_text,
			.destroy = NULL
		}
	},
// Standard X.233 options
	{
		.id = 0xc3,
		.val = &(tlv2_type_descriptor_t){
			.label = "QoS maintenance",
			.parse = tlv2_octet_string_parse,
			.format_text = tlv2_octet_string_format_text,
			.destroy = NULL
		 },
	},
	{
		.id = 0xc1,
		.val = &(tlv2_type_descriptor_t){
			.label = "Discard reason",
			.parse = clnp_error_code_parse,
			.format_text = clnp_error_code_format_text,
			.destroy = NULL
		},
	},
	{
		.id = 0xc4,
		.val = &(tlv2_type_descriptor_t){
			.label = "Prefix-based scope control",
			.parse = tlv2_octet_string_parse,
			.format_text = tlv2_octet_string_format_text,
			.destroy = NULL
		 },
	},
	{
		.id = 0xc5,
		.val = &(tlv2_type_descriptor_t){
			.label = "Security",
			.parse = tlv2_octet_string_parse,
			.format_text = tlv2_octet_string_format_text,
			.destroy = NULL
		 },
	},
	{
		.id = 0xc6,
		.val = &(tlv2_type_descriptor_t){
			.label = "Radius scope control",
			.parse = tlv2_octet_string_parse,
			.format_text = tlv2_octet_string_format_text,
			.destroy = NULL
		 },
	},
	{
		.id = 0xc8,
		.val = &(tlv2_type_descriptor_t){
			.label = "Source routing",
			.parse = tlv2_octet_string_parse,
			.format_text = tlv2_octet_string_format_text,
			.destroy = NULL
		 },
	},
	{
		.id = 0xcb,
		.val = &(tlv2_type_descriptor_t){
			.label = "Record route",
			.parse = tlv2_octet_string_parse,
			.format_text = tlv2_octet_string_format_text,
			.destroy = NULL
		 },
	},
	{
		.id = 0xcc,
		.val = &(tlv2_type_descriptor_t){
			.label = "Padding",
			.parse = tlv2_octet_string_parse,
			.format_text = tlv2_octet_string_format_text,
			.destroy = NULL
		 },
	},
	{
		.id = 0xcd,
		.val = &(tlv2_type_descriptor_t){
			.label = "Priority",
			.parse = tlv2_octet_string_parse,
			.format_text = tlv2_octet_string_format_text,
			.destroy = NULL
		 },
	},
	{
		.id = 0x0,
		.val = NULL
	}
};


la_proto_node *clnp_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	clnp_pdu_t *pdu = XCALLOC(1, sizeof(clnp_pdu_t));
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_clnp_pdu;
	node->data = pdu;
	node->next = NULL;

	pdu->err = true;	// fail-safe default
	uint8_t *ptr = buf;
	uint32_t remaining = len;
	if(remaining < CLNP_MIN_LEN) {
		debug_print("Too short (len %u < min len %u)\n", remaining, CLNP_MIN_LEN);
		goto fail;
	}

	CAST_PTR(hdr, clnp_hdr_t *, ptr);
	pdu->hdr = hdr;
	if(hdr->len == 255) {
		debug_print("%s", "invalid length indicator - value 255 is reserved\n");
		goto fail;
	}
	if(remaining < hdr->len) {
		debug_print("header truncated: buf_len %u < len_indicator %u\n", remaining, hdr->len);
		goto fail;
	}
	if(hdr->version != 1) {
		debug_print("unsupported PDU version %u\n", hdr->version);
		goto fail;
	}
	pdu->lifetime_sec = (float)hdr->lifetime * 0.5f;
	pdu->seg_len = extract_uint16_msbfirst(hdr->seg_len);
	pdu->cksum = extract_uint16_msbfirst(hdr->cksum);
	debug_print("seg_len: %u, cksum: 0x%x\n", pdu->seg_len, pdu->cksum);
	ptr += sizeof(clnp_hdr_t); remaining -= sizeof(clnp_hdr_t);

	int ret = octet_string_parse(ptr, remaining, &(pdu->dst_nsap));
	if(ret < 0) {
		debug_print("%s", "failed to parse dst NET addr\n");
		goto fail;
	}
	ptr += ret; remaining -= ret;
	debug_print("dst NET: consumed %d octets, remaining: %u\n", ret, remaining);

	ret = octet_string_parse(ptr, remaining, &(pdu->src_nsap));
	if(ret < 0) {
		debug_print("%s", "failed to parse src NET addr\n");
		goto fail;
	}
	ptr += ret; remaining -= ret;
	debug_print("src NET: consumed %d octets, remaining: %u\n", ret, remaining);

	if(hdr->sp != 0) {	// segmentation part is present
		if(remaining < 6) {
			debug_print("segmentation part truncated: len %u < required 6\n", remaining);
			goto fail;
		}
		pdu->pdu_id = extract_uint16_msbfirst(ptr);
		pdu->seg_off = extract_uint16_msbfirst(ptr + 2);
		pdu->total_init_pdu_len = extract_uint16_msbfirst(ptr + 4);
		ptr += 6; remaining -= 6;
	}

	int options_part_len = hdr->len - (ptr - buf);
	debug_print("options_part_len: %d\n", options_part_len);
	if(options_part_len > 0) {
		pdu->options = tlv2_parse(ptr, (uint16_t)options_part_len, clnp_options, 1);
		if(pdu->options == NULL) {
			debug_print("%s", "tlv2_parse failed on options part\n");
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

TLV2_PARSER(clnp_error_code_parse) {
// -Wunused-parameter
	(void)typecode;
	ASSERT(buf != NULL);

	if(len != 2) {
		return NULL;
	}
	clnp_error_t *e = XCALLOC(1, sizeof(clnp_error_t));
	e->code = buf[0];
	e->erroneous_octet = buf[1];
	return e;
}

TLV2_FORMATTER(clnp_error_code_format_text) {
	static dict const clnp_error_codes[] = {
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
	CAST_PTR(e, clnp_error_t *, data);
	char *str = dict_search(clnp_error_codes, e->code);
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: %u (%s)", label, e->code, str ? str : "unknown");
	if(e->erroneous_octet != 0) {
		la_vstring_append_sprintf(ctx->vstr, ", erroneous octet value: 0x%02x", e->erroneous_octet);
	}
	la_vstring_append_sprintf(ctx->vstr, "%s", "\n");
}

void clnp_pdu_format_text(la_vstring * const vstr, void const * const data, int indent) {
	static dict const clnp_pdu_types[] = {
		{ .id = CLNP_NPDU_DT, .val = "Data" },
		{ .id = CLNP_NDPU_MD, .val = "Multicast Data" },
		{ .id = CLNP_NDPU_ER, .val = "Error Report" },
		{ .id = CLNP_NDPU_ERP, .val = "Echo Request" },
		{ .id = CLNP_NDPU_ERQ, .val = "Echo Reply" },
		{ .id = 0, .val = NULL }
	};

	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	CAST_PTR(pdu, clnp_pdu_t *, data);
	if(pdu->err == true) {
		LA_ISPRINTF(vstr, indent, "%s", "-- Unparseable X.233 CLNP PDU\n");
		return;
	}
	char *pdu_type = dict_search(clnp_pdu_types, pdu->hdr->type);
	if(pdu_type != NULL) {
		LA_ISPRINTF(vstr, indent, "X.233 CLNP %s:\n", pdu_type);
	} else {
		LA_ISPRINTF(vstr, indent, "X.233 CLNP unknown PDU (code=0x%02x):\n", pdu->hdr->type);
	}
	indent++;

	char *str = fmt_hexstring_with_ascii(pdu->src_nsap.buf, pdu->src_nsap.len);
	LA_ISPRINTF(vstr, indent, "Src NSAP: %s\n", str);
	XFREE(str);
	str = fmt_hexstring_with_ascii(pdu->dst_nsap.buf, pdu->dst_nsap.len);
	LA_ISPRINTF(vstr, indent, "Dst NSAP: %s\n", str);
	XFREE(str);

	LA_ISPRINTF(vstr, indent, "Lifetime: %.1f sec\n", pdu->lifetime_sec);
	LA_ISPRINTF(vstr, indent, "Flags:%s%s%s\n",
		pdu->hdr->sp ? " SP" : "",
		pdu->hdr->ms ? " MS" : "",
		pdu->hdr->er ? " E/R" : "");
//	LA_ISPRINTF(vstr, indent, "Segment length: %u\n", pdu->seg_len);
//	LA_ISPRINTF(vstr, indent, "Checksum: %x\n", pdu->cksum);

	if(pdu->hdr->sp != 0) {
		LA_ISPRINTF(vstr, indent, "%s", "Segmentation:\n");
		indent++;
		LA_ISPRINTF(vstr, indent, "PDU Id: 0x%x\n", pdu->pdu_id);
		LA_ISPRINTF(vstr, indent, "Segment offset: %u\n", pdu->seg_off);
		LA_ISPRINTF(vstr, indent, "PDU total length: %u\n", pdu->total_init_pdu_len);
		indent--;
	}
	if(pdu->options != NULL) {
		LA_ISPRINTF(vstr, indent, "%s", "Options:\n");
		tlv2_list_format_text(vstr, pdu->options, indent+1);
	}
	if(pdu->hdr->type == CLNP_NDPU_ER) {
		LA_ISPRINTF(vstr, indent-1, "%s", "Erroneous NPDU:\n");
	}
}

void clnp_pdu_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	CAST_PTR(pdu, clnp_pdu_t *, data);
	tlv2_list_destroy(pdu->options);
	pdu->options = NULL;
	XFREE(data);
}

la_type_descriptor const proto_DEF_clnp_pdu = {
	.format_text = clnp_pdu_format_text,
	.destroy = clnp_pdu_destroy,
};

/**********************************
 * Compressed CLNP NPDU decoder
 **********************************/
//
// Forward declaration
la_type_descriptor const proto_DEF_clnp_compressed_init_data_pdu;

la_proto_node *clnp_compressed_init_data_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	clnp_compressed_init_data_pdu_t *pdu = XCALLOC(1, sizeof(clnp_compressed_init_data_pdu_t));
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_clnp_compressed_init_data_pdu;
	node->data = pdu;
	node->next = NULL;

	pdu->err = true;			// fail-safe default
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

void clnp_compressed_init_data_pdu_format_text(la_vstring * const vstr, void const * const data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	CAST_PTR(pdu, clnp_compressed_init_data_pdu_t *, data);
	if(pdu->err == true) {
		LA_ISPRINTF(vstr, indent, "%s", "-- Unparseable X.233 CLNP compressed header PDU\n");
		return;
	}
	LA_ISPRINTF(vstr, indent, "%s", "X.233 CLNP Data (compressed header):\n");
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
