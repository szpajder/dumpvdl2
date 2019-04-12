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
#include "esis.h"
#include "dumpvdl2.h"
#include "tlv.h"

// Forward declaration
la_type_descriptor const proto_DEF_esis_pdu;

const dict atn_traffic_types[] = {
	{  1, "ATS" },
	{  2, "AOC" },
	{  4, "ATN Administrative" },
	{  8, "General Comms" },
	{ 16, "ATN System Mgmt" },
	{  0, NULL }
};

const dict atsc_traffic_classes[] = {
	{   1, "A" },
	{   2, "B" },
	{   4, "C" },
	{   8, "D" },
	{  16, "E" },
	{  32, "F" },
	{  64, "G" },
	{ 128, "H" },
	{  0, NULL }
};

static char *fmt_subnet_caps(uint8_t *data, uint16_t len) {
	if(len < 1) return strdup("<empty>");
	char *tr_types = NULL, *tr_classes = NULL;
	if((data[0] & 0x1f) == 0x1f) {
		tr_types = strdup("all");
	} else {
		tr_types = fmt_bitfield(data[0], atn_traffic_types);
	}
	if(data[0] & 1 && len > 1) {	/* ATS traffic allowed - next octet is present and contains ATSC classes */
		if(data[1] == 0xff) {
			tr_classes = strdup("all");
		} else {
			tr_classes = fmt_bitfield(data[1], atsc_traffic_classes);
		}
	}

	char *buf = XCALLOC(512, sizeof(char));
	sprintf(buf, "Permitted traffic: %s%s", tr_types, tr_classes ? " (supported ATSC classes: " : "");
	if(tr_classes) {
		strcat(buf, tr_classes);
		strcat(buf, ")");
		XFREE(tr_classes);
	}
	XFREE(tr_types);
	return buf;
}

static const dict esis_pdu_types[] = {
	{ ESIS_PDU_TYPE_ESH,	"ES Hello" },
	{ ESIS_PDU_TYPE_ISH,	"IS Hello" },
	{ 0,			NULL }
};

static const tlv_dict esis_option_names[] = {
	{ 0xc5,	&fmt_hexstring,		"Security" },
	{ 0xcf, &fmt_hexstring,		"Priority" },
/* QoS Maintenance not used in ATN (ICAO 9705 Table 5.8-2) */
	{ 0x81, &fmt_subnet_caps,	"Mobile Subnetwork Capabilities" },
	{ 0x88, &fmt_hexstring,		"ATN Data Link Capabilities" },
	{ 0,	NULL,			NULL }
};

static int parse_octet_string(uint8_t *buf, uint32_t len, uint8_t **dst, uint8_t *dstlen) {
	if(len == 0) {
		debug_print("%s", "empty buffer\n");
		return -1;
	}
	uint8_t buflen = *buf++; len--;
	if(len < buflen) {
		debug_print("buffer truncated: len %u < expected %u\n", len, buflen);
		return -1;
	}
	*dst = buf;
	*dstlen = buflen;
	return 1 + buflen;	// total number of consumed octets
}

la_proto_node *esis_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	esis_pdu_t *pdu = XCALLOC(1, sizeof(esis_pdu_t));
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

	int ret = parse_octet_string(ptr, remaining, &pdu->net_addr, &pdu->net_addr_len);
	if(ret < 0) {
		goto end;
	}
	ptr += ret; remaining -= ret;
	switch(hdr->type) {
	case ESIS_PDU_TYPE_ESH:
	case ESIS_PDU_TYPE_ISH:
		if(remaining > 0) {
			pdu->options = tlv_deserialize(ptr, remaining, 1);
			if(pdu->options == NULL) {
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

	char *str = fmt_hexstring_with_ascii(pdu->net_addr, pdu->net_addr_len);
	switch(hdr->type) {
	case ESIS_PDU_TYPE_ESH:
		LA_ISPRINTF(vstr, indent, "SA : %s\n", str);
		break;
	case ESIS_PDU_TYPE_ISH:
		LA_ISPRINTF(vstr, indent, "NET: %s\n", str);
		break;
	}
	XFREE(str);
	if(pdu->options != NULL) {
		LA_ISPRINTF(vstr, indent, "%s", "Options:\n");
		tlv_format_as_text(vstr, pdu->options, esis_option_names, indent+1);
	}
}

void esis_pdu_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	CAST_PTR(pdu, esis_pdu_t *, data);
	tlv_list_free(pdu->options);
	pdu->options = NULL;
	XFREE(data);
}

la_type_descriptor const proto_DEF_esis_pdu = {
	.format_text = esis_pdu_format_text,
	.destroy = esis_pdu_destroy
};
