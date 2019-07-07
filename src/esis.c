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
#include <libacars/libacars.h>	// la_type_descriptor, la_proto_node
#include <libacars/vstring.h>	// la_vstring, LA_ISPRINTF()
#include "atn.h"		// atn_traffic_types, atsc_traffic_classes
#include "esis.h"
#include "dumpvdl2.h"
#include "tlv.h"

// Forward declaration
la_type_descriptor const proto_DEF_esis_pdu;

typedef struct {
	uint8_t atn_traffic_types;
	uint8_t atsc_traffic_classes;
	bool atsc_traffic_classes_present;
} esis_subnet_caps_t;

TLV_PARSER(esis_subnet_caps_parse) {
	UNUSED(typecode);
	if(len < 1) return NULL;

	NEW(esis_subnet_caps_t, ret);
	ret->atn_traffic_types = buf[0];
	ret->atsc_traffic_classes_present = false;
	if(buf[0] & 1 && len > 1) {	/* ATS traffic allowed - next octet is present and contains ATSC classes */
		ret->atsc_traffic_classes = buf[1];
		ret->atsc_traffic_classes_present = true;
	}
	return ret;
}

TLV_FORMATTER(esis_subnet_caps_format_text) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);

	CAST_PTR(c, esis_subnet_caps_t *, data);
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s:\n", label);
	LA_ISPRINTF(ctx->vstr, ctx->indent+1, "%s: ", "Permitted traffic");
	if((c->atn_traffic_types & ATN_TRAFFIC_TYPES_ALL) == ATN_TRAFFIC_TYPES_ALL) {
		la_vstring_append_sprintf(ctx->vstr, "%s", "all");
	} else {
		bitfield_format_text(ctx->vstr, c->atn_traffic_types, atn_traffic_types);
	}
	EOL(ctx->vstr);
	if(c->atsc_traffic_classes_present) {
		LA_ISPRINTF(ctx->vstr, ctx->indent+1, "%s: ", "Supported ATSC classes");
		if((c->atsc_traffic_classes & ATSC_TRAFFIC_CLASSES_ALL) == ATSC_TRAFFIC_CLASSES_ALL) {
			la_vstring_append_sprintf(ctx->vstr, "%s", "all");
		} else {
			bitfield_format_text(ctx->vstr, c->atsc_traffic_classes, atsc_traffic_classes);
		}
	}
	EOL(ctx->vstr);
}

static const dict esis_pdu_types[] = {
	{ ESIS_PDU_TYPE_ESH,	"ES Hello" },
	{ ESIS_PDU_TYPE_ISH,	"IS Hello" },
	{ 0,			NULL }
};

static const dict esis_options[] = {
	{
		.id = 0xc5,
		.val = &(tlv_type_descriptor_t){
			.label = "Security",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 0xcf,
		.val = &(tlv_type_descriptor_t){
			.label = "Priority",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		}
	},
/* QoS Maintenance not used in ATN (ICAO 9705 Table 5.8-2) */
	{
		.id = 0x81,
		.val = &(tlv_type_descriptor_t){
			.label = "Mobile Subnetwork Capabilities",
			.parse = esis_subnet_caps_parse,
			.format_text = esis_subnet_caps_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 0x88,
		.val = &(tlv_type_descriptor_t){
			.label = "ATN Data Link Capabilities",
			.parse = tlv_octet_string_parse,
			.format_text = tlv_octet_string_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 0,
		.val = NULL
	}
};

la_proto_node *esis_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	NEW(esis_pdu_t, pdu);
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_esis_pdu;
	node->data = pdu;
	node->next = NULL;

	pdu->err = true;		// fail-safe default
	uint8_t *ptr = buf;
	uint32_t remaining = len;
	if(remaining < ESIS_HDR_LEN) {
		debug_print("Too short (len %u < min len %u)\n", remaining, ESIS_HDR_LEN);
		goto end;
	}
	esis_hdr_t *hdr = (esis_hdr_t *)ptr;
	if(hdr->version != 1) {
		debug_print("Unsupported PDU version %u\n", hdr->version);
		goto end;
	}
	pdu->holdtime = ((uint16_t)hdr->holdtime[0] << 8) | ((uint16_t)hdr->holdtime[1]);
	debug_print("pid: %02x len: %u type: %u holdtime: %u\n",
		hdr->pid, hdr->len, hdr->type, pdu->holdtime);
	if(remaining < hdr->len) {
		debug_print("Too short (len %u < PDU len %u)\n", remaining, hdr->len);
		goto end;
	}
	ptr += ESIS_HDR_LEN; remaining -= ESIS_HDR_LEN;
	debug_print("skipping %u hdr octets, len is now %u\n", ESIS_HDR_LEN, remaining);

	int ret = octet_string_parse(ptr, remaining, &pdu->net_addr);
	if(ret < 0) {
		goto end;
	}
	ptr += ret; remaining -= ret;
	switch(hdr->type) {
	case ESIS_PDU_TYPE_ESH:
	case ESIS_PDU_TYPE_ISH:
		if(remaining > 0) {
			pdu->options = tlv_parse(ptr, remaining, esis_options, 1);
			if(pdu->options == NULL) {
				debug_print("%s", "tlv_parse failed when parsing options\n");
				goto end;
			}
		}
		break;
	default:
		debug_print("Unknown PDU type 0x%02x\n", hdr->type);
		goto end;
	}
	pdu->hdr = hdr;
	*msg_type |= MSGFLT_ESIS;
	pdu->err = false;
	return node;
end:
	node->next = unknown_proto_pdu_new(buf, len);
	return node;
}

static void esis_pdu_format_text(la_vstring * const vstr, void const * const data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	CAST_PTR(pdu, esis_pdu_t *, data);
	if(pdu->err == true) {
		LA_ISPRINTF(vstr, indent, "%s", "-- Unparseable ES-IS PDU\n");
		return;
	}
	esis_hdr_t *hdr = pdu->hdr;
	char *pdu_name = (char *)dict_search(esis_pdu_types, hdr->type);
	LA_ISPRINTF(vstr, indent, "ES-IS %s: Hold Time: %u sec\n", pdu_name, pdu->holdtime);
	indent++;

	switch(hdr->type) {
	case ESIS_PDU_TYPE_ESH:
		LA_ISPRINTF(vstr, indent, "%s", "SA : ");
		break;
	case ESIS_PDU_TYPE_ISH:
		LA_ISPRINTF(vstr, indent, "%s", "NET: ");
		break;
	}
	octet_string_with_ascii_format_text(vstr, &pdu->net_addr, 0);
	EOL(vstr);
	if(pdu->options != NULL) {
		LA_ISPRINTF(vstr, indent, "%s", "Options:\n");
		tlv_list_format_text(vstr, pdu->options, indent+1);
	}
}

void esis_pdu_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	CAST_PTR(pdu, esis_pdu_t *, data);
	tlv_list_destroy(pdu->options);
	pdu->options = NULL;
	XFREE(data);
}

la_type_descriptor const proto_DEF_esis_pdu = {
	.format_text = esis_pdu_format_text,
	.destroy = esis_pdu_destroy
};
