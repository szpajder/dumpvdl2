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
#include "idrp.h"
#include "dumpvdl2.h"
#include "tlv.h"

// Forward declaration
la_type_descriptor const proto_DEF_idrp_pdu;

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

static char *fmt_route_separator(uint8_t *data, uint16_t len) {
	char *buf = XCALLOC(64, sizeof(char));
	if(len != 5) {
		sprintf(buf, "(incorrect length %u)", len);
	} else {
		uint32_t id = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
		sprintf(buf, "ID: %u, Local preference: %u", id, data[4]);
	}
	return buf;
}

static const tlv_dict path_attribute_names[] = {
	{ 1,	&fmt_route_separator,	"Route" },
	{ 2,	&fmt_hexstring,		"Ext. info" },
	{ 3,	&fmt_hexstring_with_ascii,	"RD path" },
	{ 4,	&fmt_hexstring,		"Next hop" },
	{ 5,	&fmt_hexstring,		"Distribute list inclusions" },
	{ 6,	&fmt_hexstring,		"Distribute list exclusions" },
	{ 7,	&fmt_hexstring,		"Multi exit discriminator" },
	{ 8,	&fmt_hexstring,		"Transit delay" },
	{ 9,	&fmt_hexstring,		"Residual error" },
	{ 10,	&fmt_hexstring,		"Expense" },
	{ 11,	&fmt_hexstring,		"Locally defined QoS" },
	{ 12,	&fmt_hexstring,		"Hierarchical recording" },
	{ 13,	&fmt_hexstring,		"RD hop count" },
	{ 14,	&fmt_hexstring,		"Security" },
	{ 15,	&fmt_hexstring,		"Capacity" },
	{ 16,	&fmt_hexstring,		"Priority" },
	{ 0,    NULL,			NULL }
};

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
	pdu->open_holdtime = ((uint16_t)buf[0] << 8) | ((uint16_t)buf[1]);
	buf += 2; len -= 2;
	pdu->open_max_pdu_size = ((uint16_t)buf[0] << 8) | ((uint16_t)buf[1]);
	buf += 2; len -= 2;
	pdu->open_src_rdi_len = *buf++; len--;
	if(len < pdu->open_src_rdi_len) {
		debug_print("Truncated source RDI: len %u < rdi_len %u\n", len, pdu->open_src_rdi_len);
		return -1;
	}
	pdu->open_src_rdi = buf;
	buf += pdu->open_src_rdi_len; len -= pdu->open_src_rdi_len;
// TODO: Rib-AttsSet, Auth Code, Auth Data
	pdu->data = buf; pdu->datalen = len;
	return 0;
}

static int parse_idrp_update_pdu(idrp_pdu_t *pdu, uint8_t *buf, uint32_t len) {
	if(len < 4) {
		debug_print("Truncated Update BISPDU: len %u < 4\n", len);
		return -1;
	}
	uint16_t num_withdrawn = ((uint16_t)buf[0] << 8) | ((uint16_t)buf[1]);
	buf += 2; len -= 2;
	if(num_withdrawn > 0) {
		if(len < num_withdrawn * 4) {
			debug_print("Withdrawn Routes field truncated: len %u < expected %u\n", len, 4 * num_withdrawn);
			return -1;
		}
		for(int i = 0; i < num_withdrawn; i++) {
			tlv_list_append(&pdu->withdrawn_routes, 0xff, 4, buf);	// type field is irrelevant here
			buf += 4; len -= 4;
		}
	}
	if(len < 2) {
		debug_print("BISPDU truncated after withdrawn routes: len %u < 2\n", len);
		return -1;
	}
	uint16_t total_attrib_len = ((uint16_t)buf[0] << 8) | ((uint16_t)buf[1]);
	buf += 2; len -= 2;
	if(total_attrib_len > 0) {
		if(len < total_attrib_len) {
			debug_print("Path attributes field truncated: len %u < expected %u\n", len, total_attrib_len);
			return -1;
		}
		while(total_attrib_len > 4) {			// flag + type + length
			buf++; len--; total_attrib_len--; 	// flag is not too interesting...
			uint8_t type = *buf++; len--; total_attrib_len--;
			uint16_t alen = ((uint16_t)buf[0] << 8) | ((uint16_t)buf[1]);
			buf += 2; len -= 2; total_attrib_len -= 2;
			if(len < alen) {
				debug_print("Attribute value truncated: len %u < expected %u\n", len, alen);
				return -1;
			}
// TODO: parse RD_PATH
			tlv_list_append(&pdu->path_attributes, type, alen, buf);
			buf += alen; len -= alen; total_attrib_len -= alen;
		}
		if(total_attrib_len > 0) {
			debug_print("total_attrib_len disagrees with length of the attributes: (%u octets left)\n", total_attrib_len);
			return -1;
		}
	}
// TODO: parse NLRI
	pdu->data = buf; pdu->datalen = len;
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
	pdu->data = buf;
	pdu->datalen = len;
	return 0;
}

la_proto_node *idrp_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	idrp_pdu_t *pdu = XCALLOC(1, sizeof(idrp_pdu_t));
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
	uint16_t pdu_len = ((uint16_t)hdr->len[0] << 8) | ((uint16_t)hdr->len[1]);
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
	if(pdu->data != NULL && pdu->datalen > 0) {
		append_hexstring_with_indent(vstr, pdu->data, pdu->datalen, indent);
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
			char *fmt = fmt_hexstring_with_ascii(pdu->open_src_rdi, pdu->open_src_rdi_len);
			LA_ISPRINTF(vstr, indent, "Source RDI: %s\n", fmt);
			XFREE(fmt);
		}
		break;
	case BISPDU_TYPE_UPDATE:
		if(pdu->withdrawn_routes != NULL) {
			LA_ISPRINTF(vstr, indent, "%s", "Withdrawn Routes:\n");
			for(tlv_list_t *p = pdu->withdrawn_routes; p != NULL; p = p->next) {
				char *fmt = fmt_hexstring(p->val, p->len);
				LA_ISPRINTF(vstr, indent+1, "%s\n", fmt);
				XFREE(fmt);
			}
		}
		if(pdu->path_attributes != NULL) {
			tlv_format_as_text(vstr, pdu->path_attributes, path_attribute_names, indent);
		}

		if(pdu->datalen > 0) {
			char *fmt = fmt_hexstring_with_ascii(pdu->data, pdu->datalen);
			LA_ISPRINTF(vstr, indent, "NLRI: %s\n", fmt);
			XFREE(fmt);
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
	tlv_list_free(pdu->withdrawn_routes);
	tlv_list_free(pdu->path_attributes);
	XFREE(data);
}

la_type_descriptor const proto_DEF_idrp_pdu = {
	.format_text = idrp_pdu_format_text,
	.destroy = idrp_pdu_destroy
};
