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
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <libacars/libacars.h>		// la_proto_node
#include <libacars/vstring.h>		// la_vstring
#include <libacars/list.h>		// la_list
#include "idrp.h"
#include "dumpvdl2.h"
#include "tlv.h"
#include "atn.h"			// atn_sec_label_parse, atn_sec_label_format_text

// Forward declarations
la_type_descriptor const proto_DEF_idrp_pdu;
static tlv_type_descriptor_t tlv_DEF_idrp_attr_no_value;
static tlv_type_descriptor_t tlv_DEF_idrp_ribatt;

static const dict bispdu_types[] = {
	{ BISPDU_TYPE_OPEN,		"Open" },
	{ BISPDU_TYPE_UPDATE,		"Update" },
	{ BISPDU_TYPE_ERROR,		"Error" },
	{ BISPDU_TYPE_KEEPALIVE,	"Keepalive" },
	{ BISPDU_TYPE_CEASE,		"Cease" },
	{ BISPDU_TYPE_RIBREFRESH,	"RIB Refresh" },
	{ 0,				NULL }
};

static const dict open_pdu_errors[] = {
	{ 1,	"Unsupported version number" },
	{ 2,	"Bad max PDU size" },
	{ 3,	"Bad peer RD" },
	{ 4,	"Unsupported auth code" },
	{ 5,	"Auth failure" },
	{ 6,	"Bad RIB-AttsSet" },
	{ 7,	"RDC Mismatch" },
	{ 0,	NULL }
};

static const dict update_pdu_errors[] = {
	{ 1,	"Malformed attribute list" },
	{ 2,	"Unrecognized well-known attribute" },
	{ 3, 	"Missing well-known attribute" },
	{ 4,	"Attribute flags error" },
	{ 5, 	"Attribute length error" },
	{ 6, 	"RD routing loop" },
	{ 7,	"Invalid NEXT_HOP attribute" },
	{ 8,	"Optional attribute error" },
	{ 9,	"Invalid reachability information" },
	{ 10,	"Misconfigured RDCs" },
	{ 11,	"Malformed NLRI" },
	{ 12,	"Duplicated attributes" },
	{ 13,	"Illegal RD path segment" },
	{ 0,	NULL }
};

static const dict timer_expired_errors[] = {
	{ 0,	"NULL" },
	{ 0,	NULL }
};

static const dict FSM_states[] = {
	{ 1,	"CLOSED" },
	{ 2,	"OPEN-RCVD" },
	{ 3,	"OPEN-SENT" },
	{ 4,	"CLOSE-WAIT" },
	{ 5,	"ESTABLISHED" },
	{ 0,	NULL }
};

static const dict RIB_refresh_errors[] = {
	{ 1,	"Invalid opcode" },
	{ 2,	"Unsupported RIB-Atts" },
	{ 0,	NULL }
};

static const dict bispdu_errors[] = {
	{ BISPDU_ERR_OPEN_PDU,		&(bispdu_err_t){ "Open PDU error",	(dict *)&open_pdu_errors } },
	{ BISPDU_ERR_UPDATE_PDU,	&(bispdu_err_t){ "Update PDU error",	(dict *)&update_pdu_errors } },
	{ BISPDU_ERR_TIMER_EXPIRED,	&(bispdu_err_t){ "Hold timer expired",	(dict *)&timer_expired_errors } },
	{ BISPDU_ERR_FSM,		&(bispdu_err_t){ "FSM error",		(dict *)&FSM_states } },
	{ BISPDU_ERR_RIB_REFRESH_PDU,	&(bispdu_err_t){ "RIB Refresh PDU error", (dict *)&RIB_refresh_errors } },
	{ 0,				NULL }
};

typedef struct {
	uint32_t id;
	uint8_t localpref;
} idrp_route_separator_t;

TLV_PARSER(idrp_route_separator_parse) {
	UNUSED(typecode);
	if(len != 5) {
		debug_print("incorrect length: %zu != 5)", len);
		return NULL;
	}
	NEW(idrp_route_separator_t, ret);
	ret->id = extract_uint32_msbfirst(buf);
	ret->localpref = buf[4];
	return ret;
}

TLV_FORMATTER(idrp_route_separator_format_text) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);

	CAST_PTR(s, idrp_route_separator_t *, data);
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s:\n", label);
	LA_ISPRINTF(ctx->vstr, ctx->indent+1, "ID: %u\n", s->id);
	LA_ISPRINTF(ctx->vstr, ctx->indent+1, "Local preference: %u\n", s->localpref);
}

static const dict path_attributes[] = {
	{
		.id = 1,
		.val = &(tlv_type_descriptor_t){
			.label = "Route",
			.parse = idrp_route_separator_parse,
			.format_text = idrp_route_separator_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 2,
		.val = &(tlv_type_descriptor_t){
			.label = "Ext. info",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 3,
		.val = &(tlv_type_descriptor_t){
			.label = "RD path",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_with_ascii_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 4,
		.val = &(tlv_type_descriptor_t){
			.label = "Next hop",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 5,
		.val = &(tlv_type_descriptor_t){
			.label = "Distribute list inclusions",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 6,
		.val = &(tlv_type_descriptor_t){
			.label = "Distribute list exclusions",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 7,
		.val = &(tlv_type_descriptor_t){
			.label = "Multi exit discriminator",
			.parse = tlv_uint8_parse,
			.format_text = tlv_uint_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 8,
		.val = &(tlv_type_descriptor_t){
			.label = "Transit delay",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 9,
		.val = &(tlv_type_descriptor_t){
			.label = "Residual error",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 10,
		.val = &(tlv_type_descriptor_t){
			.label = "Expense",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 11,
		.val = &(tlv_type_descriptor_t){
			.label = "Locally defined QoS",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 12,
		.val = &(tlv_type_descriptor_t){
			.label = "Hierarchical recording",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 13,
		.val = &(tlv_type_descriptor_t){
			.label = "RD hop count",
			.parse = tlv_uint8_parse,
			.format_text = tlv_uint_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 14,
		.val = &(tlv_type_descriptor_t){
			.label = "Security",
			.parse = atn_sec_label_parse,
			.format_text = atn_sec_label_format_text,
			.destroy = atn_sec_label_destroy
		}
	},
	{
		.id = 15,
		.val = &(tlv_type_descriptor_t){
			.label = "Capacity",
			.parse = tlv_uint8_parse,
			.format_text = tlv_uint_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 16,
		.val = &(tlv_type_descriptor_t){
			.label = "Priority",
			.parse = tlv_uint8_parse,
			.format_text = tlv_uint_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 0,
		.val = NULL
	}
};

TLV_FORMATTER(idrp_attr_no_value_format_text) {
	UNUSED(label);
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);

// Find the attribute label and print it to indicate its presence in RIB-Att)
	CAST_PTR(typecode, uint8_t *, data);
	CAST_PTR(td, tlv_type_descriptor_t *, dict_search(path_attributes, *typecode));
	if(td != NULL) {
		LA_ISPRINTF(ctx->vstr, ctx->indent, "%s\n", td->label);
	} else {
		LA_ISPRINTF(ctx->vstr, ctx->indent, "Unknown attribute %u\n", *typecode);
	}
}

typedef struct {
	la_list *list;
	int consumed;
} attr_parse_result_t;

static attr_parse_result_t parse_idrp_ribatt(uint8_t *buf, uint32_t len) {
	la_list *attr_list = NULL;
	if(len < 1) {
		goto fail;
	}
	int consumed = 0;
	uint8_t attrs_cnt = buf[0];
	buf++; consumed++; len--;

	uint8_t i = 0;
	while(i < attrs_cnt && len > 0) {
		uint8_t typecode = buf[0];
		buf++; consumed++; len--;
		if(typecode == 11 || typecode == 14) {
// Locally Defined Qos and Security are encoded as full TLVs. Extract the length
// field and run the standard tag parser for them.
			if(len < 2) {
				goto fail;
			}
			size_t tag_len = extract_uint16_msbfirst(buf);
			buf += 2; consumed += 2; len -= 2;
			if(tag_len > len) {
				debug_print("Tag %d: parameter truncated: tag_len=%zu buflen=%u\n",
					typecode, tag_len, len);
				goto fail;
			}
			attr_list = tlv_single_tag_parse(typecode, buf, tag_len,
				path_attributes, attr_list);
			buf += tag_len; consumed += tag_len; len -= tag_len;
		} else {
// Other attributes are encoded as value only.
// Can't store a NULL value in tlv_tag_t, so just store the typecode as value
			NEW(uint8_t, u);
			*u = typecode;
			attr_list = tlv_list_append(attr_list, typecode,
				&tlv_DEF_idrp_attr_no_value, u);
		}
		i++;
	}
	return (attr_parse_result_t){ .list = attr_list, .consumed = consumed };
fail:
	tlv_list_destroy(attr_list);
	return (attr_parse_result_t){ .list = NULL, .consumed = -1 };
}

typedef struct {
	uint8_t num;		// RIBAtt index number
	la_list *attr_list;	// Attributes
} idrp_ribatt_t;

static attr_parse_result_t parse_idrp_ribatts_set(uint8_t *buf, uint32_t len) {
	la_list *ribatt_list = NULL;
	if(len < 1) {
		goto fail;
	}
	int consumed = 0;
	uint8_t ribatts_cnt = buf[0];
	buf++; consumed++; len--;

	uint8_t i = 0;
	while(i < ribatts_cnt && len > 0) {
		attr_parse_result_t result = parse_idrp_ribatt(buf, len);
		debug_print("RibAtt #%u: parse_idrp_ribatt consumed %d octets\n", i, result.consumed);
		if(result.consumed < 0) {
			goto fail;
		}
		buf += result.consumed; consumed += result.consumed; len -= result.consumed;
		NEW(idrp_ribatt_t, ribatt);
		ribatt->num = i;
		ribatt->attr_list = result.list;
		ribatt_list = tlv_list_append(ribatt_list, i, &tlv_DEF_idrp_ribatt, ribatt);
		i++;
	}
	return (attr_parse_result_t){ .list = ribatt_list, .consumed = consumed };
fail:
	tlv_list_destroy(ribatt_list);
	return (attr_parse_result_t){ .list = NULL, .consumed = -1 };
}

TLV_FORMATTER(idrp_ribatt_format_text) {
	UNUSED(label);
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);

	CAST_PTR(r, idrp_ribatt_t *, data);
	LA_ISPRINTF(ctx->vstr, ctx->indent, "RibAtt #%u:\n", r->num);
	tlv_list_format_text(ctx->vstr, r->attr_list, ctx->indent+1);
}

TLV_DESTRUCTOR(idrp_ribatt_destroy) {
	if(data == NULL) {
		return;
	}
	CAST_PTR(ribatt, idrp_ribatt_t *, data);
	tlv_list_destroy(ribatt->attr_list);
	XFREE(data);
}

// A pseudo-type which only prints its label
static tlv_type_descriptor_t tlv_DEF_idrp_attr_no_value = {
	.label = "",
	.parse = NULL,
	.format_text = idrp_attr_no_value_format_text,
	.destroy = NULL
};

static tlv_type_descriptor_t tlv_DEF_idrp_ribatt = {
	.label = "",
	.parse = NULL,
	.format_text = idrp_ribatt_format_text,
	.destroy = idrp_ribatt_destroy
};

static attr_parse_result_t parse_idrp_confed_ids(uint8_t *buf, uint32_t len) {
	la_list *confed_id_list = NULL;
	if(len < 1) {
		goto fail;
	}
	int consumed = 0;
	uint8_t confed_id_cnt = buf[0];
	buf++; consumed++; len--;

	uint8_t i = 0;
	while(i < confed_id_cnt && len > 0) {
		uint8_t confed_id_len = buf[0];
		buf++; consumed++; len--;
		if(len < confed_id_len) {
			debug_print("Truncated Confed ID #%d: len %u < confed_id_len %u\n",
				i, len, confed_id_len);
			goto fail;
		}
		confed_id_list = la_list_append(confed_id_list, octet_string_new(buf, confed_id_len));
		buf += confed_id_len; consumed += confed_id_len; len -= confed_id_len;
		i++;
	}
	return (attr_parse_result_t){ .list = confed_id_list, .consumed = consumed };
fail:
	la_list_free(confed_id_list);
	return (attr_parse_result_t){ .list = NULL, .consumed = -1 };
}

static int parse_idrp_open_pdu(idrp_pdu_t *pdu, uint8_t *buf, uint32_t len) {
	if(len < 6) {
		debug_print("Truncated Open BISPDU: len %u < 6\n", len);
		return -1;
	}
	if(*buf != BISPDU_OPEN_VERSION) {
		debug_print("Unsupported Open BISPDU version %u\n", *buf);
		return -1;
	}
	buf++; len--;
	pdu->open_holdtime = extract_uint16_msbfirst(buf);
	buf += 2; len -= 2;
	pdu->open_max_pdu_size = extract_uint16_msbfirst(buf);
	buf += 2; len -= 2;
	pdu->open_src_rdi.len = *buf++; len--;
	if(len < pdu->open_src_rdi.len) {
		debug_print("Truncated source RDI: len %u < rdi_len %zu\n", len, pdu->open_src_rdi.len);
		return -1;
	}
	pdu->open_src_rdi.buf = buf;
	buf += pdu->open_src_rdi.len; len -= pdu->open_src_rdi.len;
	attr_parse_result_t result = parse_idrp_ribatts_set(buf, len);
// FIXME: don't fail catastrophically on unparseable RibAttsSet.
// We can still print most of the dissected packet in this case.
	if(result.consumed < 0) {
		return -1;
	}
	pdu->ribatts_set = result.list;
	buf += result.consumed; len -= result.consumed;

	result = parse_idrp_confed_ids(buf, len);
	if(result.consumed < 0) {
		return -1;
	}
	pdu->confed_ids = result.list;
	buf += result.consumed; len -= result.consumed;
// TODO: Auth Code, Auth Data
	pdu->data = octet_string_new(buf, len);
	return 0;
}

static int parse_idrp_update_pdu(idrp_pdu_t *pdu, uint8_t *buf, uint32_t len) {
	if(len < 4) {
		debug_print("Truncated Update BISPDU: len %u < 4\n", len);
		return -1;
	}
	uint16_t num_withdrawn = extract_uint16_msbfirst(buf);
	buf += 2; len -= 2;
	if(num_withdrawn > 0) {
		if(len < num_withdrawn * 4) {
			debug_print("Withdrawn Routes field truncated: len %u < expected %u\n", len, 4 * num_withdrawn);
			return -1;
		}
		for(uint16_t i = 0; i < num_withdrawn; i++) {
			NEW(uint32_t, u);
			*u = extract_uint32_msbfirst(buf);
			pdu->withdrawn_routes = la_list_append(pdu->withdrawn_routes, u);
			buf += 4; len -= 4;
		}
	}
	if(len < 2) {
		debug_print("BISPDU truncated after withdrawn routes: len %u < 2\n", len);
		return -1;
	}
	uint16_t total_attrib_len = extract_uint16_msbfirst(buf);
	buf += 2; len -= 2;
	if(total_attrib_len > 0) {
		if(len < total_attrib_len) {
			debug_print("Path attributes field truncated: len %u < expected %u\n", len, total_attrib_len);
			return -1;
		}
		while(total_attrib_len > 4) {			// flag + typecode + length
			buf++; len--; total_attrib_len--; 	// flag is not too interesting...
			uint8_t typecode = *buf++; len--; total_attrib_len--;
			uint16_t alen = extract_uint16_msbfirst(buf);
			buf += 2; len -= 2; total_attrib_len -= 2;
			if(len < alen) {
				debug_print("Attribute value truncated: len %u < expected %u\n", len, alen);
				return -1;
			}
// TODO: parse RD_PATH
			pdu->path_attributes = tlv_single_tag_parse(typecode, buf, alen,
				path_attributes, pdu->path_attributes);
			buf += alen; len -= alen; total_attrib_len -= alen;
		}
		if(total_attrib_len > 0) {
			debug_print("total_attrib_len disagrees with length of the attributes: (%u octets left)\n", total_attrib_len);
			return -1;
		}
	}
// TODO: parse NLRI
	pdu->data = octet_string_new(buf, len);
	return 0;
}

static int parse_idrp_error_pdu(idrp_pdu_t *pdu, uint8_t *buf, uint32_t len) {
	if(len < 2) {
		debug_print("Truncated Error BISPDU: len %u < 2\n", len);
		return -1;
	}
	uint8_t err_code = *buf++;
	uint8_t err_subcode = *buf++;
	len -= 2;

	debug_print("code=%u subcode=%u\n", err_code, err_subcode);
	if(err_code == BISPDU_ERR_FSM) {
// upper nibble of subcode contains BISPDU type which this error PDU is related to.
// lower nibble contains current FSM state
		pdu->err_fsm_bispdu_type = err_subcode >> 4;
		pdu->err_fsm_state = err_subcode & 0xf;
	}
	pdu->err_code = err_code;
	pdu->err_subcode = err_subcode;
	pdu->data = octet_string_new(buf, len);
	return 0;
}

la_proto_node *idrp_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	NEW(idrp_pdu_t, pdu);
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_idrp_pdu;
	node->data = pdu;
	node->next = NULL;

	pdu->err = true;	// fail-safe default
	if(len < BISPDU_HDR_LEN) {
		debug_print("Too short (len %u < min len %u)\n", len, BISPDU_HDR_LEN);
		goto end;
	}
	uint8_t *ptr = buf;
	uint32_t remaining = len;
	idrp_hdr_t *hdr = (idrp_hdr_t *)ptr;
	uint16_t pdu_len = extract_uint16_msbfirst(hdr->len);
	debug_print("pid: %02x len: %u type: %u seq: %u ack: %u coff: %u cavail: %u\n",
		hdr->pid, pdu_len, hdr->type, ntohl(hdr->seq), ntohl(hdr->ack), hdr->coff, hdr->cavail);
	debug_print_buf_hex(hdr->validation, 16, "%s", "Validation:\n");
	if(remaining < pdu_len) {
		debug_print("Too short (len %u < PDU len %u)\n", remaining, pdu_len);
		goto end;
	}
	ptr += BISPDU_HDR_LEN; remaining -= BISPDU_HDR_LEN;
	debug_print("skipping %u hdr octets, %u octets remaining\n", BISPDU_HDR_LEN, remaining);
	int result = 0;
	switch(hdr->type) {
	case BISPDU_TYPE_OPEN:
		result = parse_idrp_open_pdu(pdu, ptr, remaining);
		break;
	case BISPDU_TYPE_UPDATE:
		result = parse_idrp_update_pdu(pdu, ptr, remaining);
		break;
	case BISPDU_TYPE_ERROR:
		result = parse_idrp_error_pdu(pdu, ptr, remaining);
		break;
	case BISPDU_TYPE_KEEPALIVE:
	case BISPDU_TYPE_CEASE:
		break;
	case BISPDU_TYPE_RIBREFRESH:
		break;
	default:
		debug_print("Unknown BISPDU type 0x%02x\n", hdr->type);
		result = -1;
	}
	if(result < 0) {		// unparseable PDU
		goto end;
	}

	if(hdr->type == BISPDU_TYPE_KEEPALIVE) {
		*msg_type |= MSGFLT_IDRP_KEEPALIVE;
	} else {
		*msg_type |= MSGFLT_IDRP_NO_KEEPALIVE;
	}

	pdu->hdr = hdr;
	pdu->err = false;
	return node;
end:
	node->next = unknown_proto_pdu_new(buf, len);
	return node;
}

static void idrp_error_format_text(la_vstring *vstr, idrp_pdu_t *pdu, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(pdu != NULL);
	ASSERT(indent >= 0);

	bispdu_err_t *err = (bispdu_err_t *)dict_search(bispdu_errors, pdu->err_code);
	LA_ISPRINTF(vstr, indent, "Code: %u (%s)\n", pdu->err_code, err ? err->descr : "unknown");
	if(!err) {
		LA_ISPRINTF(vstr, indent, "Subcode: %u (unknown)\n", pdu->err_subcode);
		goto print_err_payload;
	}
	if(pdu->err_code == BISPDU_ERR_FSM) {	// special case
		char *bispdu_name = (char *)dict_search(bispdu_types, pdu->err_fsm_bispdu_type);
		char *fsm_state_name = (char *)dict_search(FSM_states, pdu->err_fsm_state);
		LA_ISPRINTF(vstr, indent, "Erroneous BISPDU type: %s\n",
			bispdu_name ? bispdu_name : "unknown");
		LA_ISPRINTF(vstr, indent, "FSM state: %s\n",
			fsm_state_name ? fsm_state_name : "unknown");
	} else {
		char *subcode = (char *)dict_search(err->subcodes, pdu->err_subcode);
		LA_ISPRINTF(vstr, indent, "Subcode: %u (%s)\n", pdu->err_subcode, subcode ? subcode : "unknown");
	}
print_err_payload:
	if(pdu->data != NULL && pdu->data->buf != NULL && pdu->data->len > 0) {
		octet_string_format_text(vstr, pdu->data, indent);
		EOL(vstr);
	}
}

void idrp_pdu_format_text(la_vstring * const vstr, void const * const data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	CAST_PTR(pdu, idrp_pdu_t *, data);
	if(pdu->err == true) {
		LA_ISPRINTF(vstr, indent, "%s", "-- Unparseable IDRP PDU\n");
		return;
	}
	idrp_hdr_t *hdr = pdu->hdr;
	char *bispdu_name = (char *)dict_search(bispdu_types, hdr->type);
	LA_ISPRINTF(vstr, indent, "IDRP %s: seq: %u ack: %u credit_offered: %u credit_avail: %u\n",
		bispdu_name, ntohl(hdr->seq), ntohl(hdr->ack), hdr->coff, hdr->cavail);
	indent++;
	switch(pdu->hdr->type) {
	case BISPDU_TYPE_OPEN:
		LA_ISPRINTF(vstr, indent, "Hold Time: %u seconds\n", pdu->open_holdtime);
		LA_ISPRINTF(vstr, indent, "Max. PDU size: %u octets\n", pdu->open_max_pdu_size);
		{
			LA_ISPRINTF(vstr, indent, "%s: ", "Source RDI");
			octet_string_with_ascii_format_text(vstr, &pdu->open_src_rdi, 0);
			EOL(vstr);
			LA_ISPRINTF(vstr, indent, "%s:\n", "RIB Attribute Set");
			if(pdu->ribatts_set != NULL) {
				tlv_list_format_text(vstr, pdu->ribatts_set, indent+1);
			} else {
				LA_ISPRINTF(vstr, indent, "%s\n", "-- Unparseable RibAttsSet field\n");
			}
			if(pdu->confed_ids != NULL) {
				LA_ISPRINTF(vstr, indent, "%s:\n", "Confederation IDs");
				indent++;
				for(la_list *p = pdu->confed_ids; p != NULL; p = p->next) {
					octet_string_with_ascii_format_text(vstr, p->data, indent);
					EOL(vstr);
				}
				indent--;
			}
			if(pdu->data != NULL && pdu->data->buf != NULL && pdu->data->len > 0) {
				octet_string_format_text(vstr, pdu->data, indent);
				EOL(vstr);
			}
		}
		break;
	case BISPDU_TYPE_UPDATE:
		if(pdu->withdrawn_routes != NULL) {
			LA_ISPRINTF(vstr, indent, "%s", "Withdrawn Routes:\n");
			indent++;
			for(la_list *p = pdu->withdrawn_routes; p != NULL; p = p->next) {
				LA_ISPRINTF(vstr, indent, "ID: %lu\n", *(uint32_t *)(p->data));
			}
			indent--;
		}
		if(pdu->path_attributes != NULL) {
			tlv_list_format_text(vstr, pdu->path_attributes, indent);
		}

		if(pdu->data != NULL && pdu->data->buf != NULL && pdu->data->len > 0) {
			LA_ISPRINTF(vstr, indent, "%s: ", "NLRI");
			octet_string_with_ascii_format_text(vstr, pdu->data, 0);
			EOL(vstr);
		}
		break;
	case BISPDU_TYPE_ERROR:
		idrp_error_format_text(vstr, pdu, indent);
		break;
	case BISPDU_TYPE_KEEPALIVE:
	case BISPDU_TYPE_CEASE:
		break;
	case BISPDU_TYPE_RIBREFRESH:
		break;
	}
}

void idrp_pdu_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	CAST_PTR(pdu, idrp_pdu_t *, data);
	la_list_free(pdu->withdrawn_routes);
	tlv_list_destroy(pdu->path_attributes);
	tlv_list_destroy(pdu->ribatts_set);
	la_list_free(pdu->confed_ids);
	XFREE(pdu->data);
	XFREE(data);
}

la_type_descriptor const proto_DEF_idrp_pdu = {
	.format_text = idrp_pdu_format_text,
	.destroy = idrp_pdu_destroy
};
