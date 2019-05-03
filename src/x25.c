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
#include "config.h"		// IS_BIG_ENDIAN
#include "dumpvdl2.h"
#include "x25.h"
#include "clnp.h"
#include "esis.h"
#include "tlv.h"

static const dict x25_pkttype_names[] = {
	{ X25_CALL_REQUEST,	"Call Request" },
	{ X25_CALL_ACCEPTED,	"Call Accepted" },
	{ X25_CLEAR_REQUEST,	"Clear Request" },
	{ X25_CLEAR_CONFIRM,	"Clear Confirm" },
	{ X25_DATA,		"Data" },
	{ X25_RR,		"Receive Ready" },
	{ X25_REJ,		"Receive Reject" },
	{ X25_RESET_REQUEST,	"Reset Request" },
	{ X25_RESET_CONFIRM,	"Reset Confirm" },
	{ X25_RESTART_REQUEST,	"Restart Request" },
	{ X25_RESTART_CONFIRM,	"Restart Confirm" },
	{ X25_DIAG,		"Diagnostics" },
	{ 0,			NULL }
};

static const tlv_dict x25_facility_names[] = {
	{ 0x00, &fmt_hexstring, "Marker (non-X.25 facilities follow)" },
	{ 0x01, &fmt_hexstring, "Fast Select" },
	{ 0x08, &fmt_hexstring, "Called line address modified" },
	{ 0x42, &fmt_hexstring, "Packet size" },
	{ 0x43, &fmt_hexstring, "Window size" },
	{ 0xc9, &fmt_hexstring_with_ascii, "Called address extension" },
	{ 0,    NULL,		NULL }
};

static const dict x25_comp_algos[] = {
	{ 0x40, "ACA" },
	{ 0x20, "DEFLATE" },
	{ 0x02, "LREF" },
	{ 0x01, "LREF-CAN" },
	{ 0x0,  NULL }
};


/**********************************************************
 * SNDCF Error Report decoder
 **********************************************************/

// Forward declarations
la_type_descriptor const proto_DEF_X25_SNDCF_error_report;
static la_proto_node *parse_x25_user_data(uint8_t *buf, uint32_t len, uint32_t *msg_type);

typedef struct {
	uint8_t error_code;
	uint8_t local_ref;
	bool err;
	bool errored_pdu_present;
} sndcf_err_rpt_t;

static la_proto_node *sndcf_error_report_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	sndcf_err_rpt_t *rpt = XCALLOC(1, sizeof(sndcf_err_rpt_t));
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_X25_SNDCF_error_report;
	node->data = rpt;
	node->next = NULL;

	rpt->err = true;		// fail-safe value
	if(len < 3) {
		debug_print("Too short (len %u < min len %u)\n", len, 3);
		goto fail;
	}
	rpt->error_code = buf[1];
	rpt->local_ref = buf[2];
	if(len > 3) {
		node->next = parse_x25_user_data(buf + 3, len - 3, msg_type);
		rpt->errored_pdu_present = true;
	} else {
		rpt->errored_pdu_present = false;
	}
	rpt->err = false;
	return node;
fail:
	node->next = unknown_proto_pdu_new(buf, len);
	return node;
}

static char const * const sndcf_error_descriptions[] = {
	[0] = "Compressed NPDU with unrecognized Local Reference",
	[1] = "Creation of directory entry outside of sender's permitted range",
	[2] = "Directory entry exists",
	[3] = "Local Reference greater than maximum value accepted",
	[4] = "Data Unit Identifier missing when SP=1",
	[5] = "reserved",
	[6] = "reserved",
	[7] = "Compressed CLNP PDU with unrecognized type",
	[8] = "Local Reference cancellation error"
};
#define SNDCF_ERR_MAX 8

void sndcf_error_report_format_text(la_vstring * const vstr, void const * const data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	CAST_PTR(rpt, sndcf_err_rpt_t *, data);
	if(rpt->err == true) {
		LA_ISPRINTF(vstr, indent, "%s", "-- Unparseable SNDCF Error Report\n");
		return;
	}
	LA_ISPRINTF(vstr, indent, "%s", "SNDCF Error Report:\n");
	LA_ISPRINTF(vstr, indent+1, "Cause: 0x%02x (%s)\n", rpt->error_code,
		rpt->error_code <= SNDCF_ERR_MAX ? sndcf_error_descriptions[rpt->error_code] : "unknown");
	LA_ISPRINTF(vstr, indent+1, "Local Reference: 0x%02x\n", rpt->local_ref);
	if(rpt->errored_pdu_present) {
		LA_ISPRINTF(vstr, indent, "%s", "Erroneous PDU:\n");
	}
}

la_type_descriptor const proto_DEF_X25_SNDCF_error_report = {
	.format_text = sndcf_error_report_format_text,
	.destroy = NULL
};

/**********************************************************
 * X.25 decoder
 **********************************************************/

// Forward declaration
la_type_descriptor const proto_DEF_X25_pkt;

static char *fmt_x25_addr(uint8_t *data, uint8_t len) {
// len is in nibbles here
	static const char hex[] = "0123456789abcdef";
	char *buf = NULL;
	if(len == 0) return strdup("none");
	if(data == NULL) return strdup("<undef>");
	uint8_t bytelen = (len >> 1) + (len & 1);
	buf = XCALLOC(2 * bytelen + 1, sizeof(char));

	char *ptr = buf;
	int i, j;
	for(i = 0, j = 0; i < bytelen; i++) {
		*ptr++ = hex[((data[i] >> 4) & 0xf)];
		if(++j == len) break;
		*ptr++ = hex[data[i] & 0xf];
		if(++j == len) break;
	}
	return buf;
}

static int parse_x25_address_block(x25_pkt_t *pkt, uint8_t *buf, uint32_t len) {
	if(len == 0) return -1;
	uint8_t calling_len = (*buf & 0xf0) >> 4;	// nibbles
	uint8_t called_len = *buf & 0x0f;		// nibbles
	uint8_t called_len_bytes = (called_len >> 1) + (called_len & 1);	// bytes
	uint8_t calling_len_bytes = (calling_len >> 1) + (calling_len & 1);	// bytes
	uint8_t addr_len = (calling_len + called_len) >> 1;			// bytes
	addr_len += (calling_len & 1) ^ (called_len & 1);	// add 1 byte if total nibble count is odd
	buf++; len--;
	debug_print("calling_len=%u called_len=%u total_len=%u len=%u\n", calling_len, called_len, addr_len, len);
	if(len < addr_len) {
		debug_print("Address block truncated (buf len %u < addr len %u)\n", len, addr_len);
		return -1;
	}
	uint8_t *abuf = pkt->called.addr;
	uint8_t *bbuf = pkt->calling.addr;
	memcpy(abuf, buf, called_len_bytes * sizeof(uint8_t));
	uint8_t calling_pos = called_len_bytes - (called_len & 1);
	memcpy(bbuf, buf + calling_pos, (addr_len - calling_pos) * sizeof(uint8_t));
	if(called_len & 1) {
		abuf[called_len_bytes-1] &= 0xf0;
// shift calling addr one nibble to the left if called addr nibble length is odd
		int i = 0;
		while(i < calling_len >> 1) {
			bbuf[i] = (bbuf[i] << 4) | (bbuf[i+1] >> 4);
			i++;
		}
		if(calling_len & 1)
			bbuf[calling_len_bytes-1] <<= 4;
	}
	pkt->called.len = called_len;
	pkt->calling.len = calling_len;
	pkt->addr_block_present = 1;
	return 1 + addr_len;	// return total number of bytes consumed
}

static int parse_x25_callreq_sndcf(x25_pkt_t *pkt, uint8_t *buf, uint32_t len) {
	if(len < 2) return -1;
	if(*buf != X25_SNDCF_ID) {
		debug_print("%s", "SNDCF identifier not found\n");
		return -1;
	}
	buf++; len--;
	uint8_t sndcf_len = *buf++; len--;
	if(sndcf_len < MIN_X25_SNDCF_LEN || *buf != X25_SNDCF_VERSION) {
		debug_print("Unsupported SNDCF field format or version (len=%u ver=%u)\n", sndcf_len, *buf);
		return -1;
	}
	if(len < sndcf_len) {
		debug_print("SNDCF field truncated (sndcf_len %u < buf_len %u)\n", sndcf_len, len);
		return -1;
	}
	pkt->compression = buf[3];
	return 2 + sndcf_len;
}

static int parse_x25_facility_field(x25_pkt_t *pkt, uint8_t *buf, uint32_t len) {
	if(len == 0) return -1;
	uint8_t fac_len = *buf;
	buf++; len--;
	if(len < fac_len) {
		debug_print("Facility field truncated (buf len %u < fac_len %u)\n", len, fac_len);
		return -1;
	}
	uint8_t i = fac_len;
	while(i > 0) {
		uint8_t code = *buf;
		uint8_t param_len = (code >> 6) & 3;
		buf++; i--;
		if(param_len < 3) {
			param_len++;
		} else {
			if(i > 0) {
				param_len = *buf++;
				i--;
			} else {
				debug_print("Facility field truncated: code=0x%02x param_len=%u, length octet missing\n",
					code, param_len);
				return -1;
			}
		}
		if(i < param_len) {
			debug_print("Facility field truncated: code=%02x param_len=%u buf len=%u\n", code, param_len, i);
			return -1;
		}
		tlv_list_append(&pkt->facilities, code, param_len, buf);
		buf += param_len; i -= param_len;
	}
	return 1 + fac_len;
}

static la_proto_node *parse_x25_user_data(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	if(buf == NULL || len == 0)
		return NULL;
	uint8_t proto = *buf;
	if(proto == SN_PROTO_CLNP) {
		return clnp_pdu_parse(buf, len, msg_type);
	} else if(proto == SN_PROTO_ESIS) {
		return esis_pdu_parse(buf, len, msg_type);
	}
	uint8_t pdu_type = proto >> 4;
	if(pdu_type < 4) {
		return clnp_compressed_init_data_pdu_parse(buf, len, msg_type);
	} else if(proto == 0xe0) {
		return sndcf_error_report_parse(buf, len, msg_type);
	}
	return unknown_proto_pdu_new(buf, len);
}

la_proto_node *x25_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	x25_pkt_t *pkt = XCALLOC(1, sizeof(x25_pkt_t));
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_X25_pkt;
	node->data = pkt;
	node->next = NULL;

	pkt->err = true;		// fail-safe value
	uint8_t *ptr = buf;
	uint32_t remaining = len;
	if(remaining < X25_MIN_LEN) {
		debug_print("Too short (len %u < min len %u)\n", remaining, X25_MIN_LEN);
		goto fail;
	}

	x25_hdr_t *hdr = (x25_hdr_t *)ptr;
	debug_print("gfi=0x%02x group=0x%02x chan=0x%02x type=0x%02x\n", hdr->gfi,
		hdr->chan_group, hdr->chan_num, hdr->type.val);
	if(hdr->gfi != GFI_X25_MOD8) {
		debug_print("Unsupported GFI 0x%x\n", hdr->gfi);
		goto fail;
	}

	ptr += sizeof(x25_hdr_t);
	remaining -= sizeof(x25_hdr_t);

	pkt->type = hdr->type.val;
// Clear out insignificant bits in pkt->type to simplify comparisons later on
// (hdr->type remains unchanged in case the original value is needed)
	uint8_t pkttype = hdr->type.val;
	if((pkttype & 1) == 0) {
		pkt->type = X25_DATA;
		*msg_type |= MSGFLT_X25_DATA;
	} else {
		pkttype &= 0x1f;
		if(pkttype == X25_RR || pkttype == X25_REJ)
			pkt->type = pkttype;
		*msg_type |= MSGFLT_X25_CONTROL;
	}
	int ret;
	switch(pkt->type) {
	case X25_CALL_REQUEST:
	case X25_CALL_ACCEPTED:
		if((ret = parse_x25_address_block(pkt, ptr, remaining)) < 0) {
			goto fail;
		}
		ptr += ret; remaining -= ret;
		if((ret = parse_x25_facility_field(pkt, ptr, remaining)) < 0) {
			goto fail;
		}
		ptr += ret; remaining -= ret;
		if(pkt->type == X25_CALL_REQUEST) {
			if((ret = parse_x25_callreq_sndcf(pkt, ptr, remaining)) < 0) {
				goto fail;
			}
			ptr += ret; remaining -= ret;
		} else if(pkt->type == X25_CALL_ACCEPTED) {
			if(remaining > 0) {
				pkt->compression = *ptr++;
				remaining--;
			} else {
				debug_print("%s", "X25_CALL_ACCEPT: no payload\n");
				goto fail;
			}
		}
	/* FALLTHROUGH */
	/* because Fast Select is on, so there might be a data PDU in call req or accept */
	case X25_DATA:
		node->next = parse_x25_user_data(ptr, remaining, msg_type);
		break;
	case X25_CLEAR_REQUEST:
		if(remaining > 0) {
			pkt->clr_cause = *ptr++;
			remaining--;
		}
		if(remaining > 0) {
			pkt->diag_code = *ptr++;
			remaining--;
		}
		break;
	case X25_CLEAR_CONFIRM:
	case X25_RR:
	case X25_REJ:
	case X25_RESET_REQUEST:
	case X25_RESET_CONFIRM:
	case X25_RESTART_REQUEST:
	case X25_RESTART_CONFIRM:
	case X25_DIAG:
		break;
	default:
		debug_print("Unsupported packet identifier 0x%02x\n", pkt->type);
		goto fail;
	}
	pkt->hdr = hdr;
	pkt->err = false;
	return node;
fail:
	node->next = unknown_proto_pdu_new(buf, len);
	return node;
}

void x25_format_text(la_vstring * const vstr, void const * const data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	CAST_PTR(pkt, x25_pkt_t *, data);
	if(pkt->err == true) {
		LA_ISPRINTF(vstr, indent, "%s", "-- Unparseable X.25 packet\n");
		return;
	}
	char *name = (char *)dict_search(x25_pkttype_names, pkt->type);
	LA_ISPRINTF(vstr, indent, "X.25 %s: grp: %u chan: %u", name, pkt->hdr->chan_group, pkt->hdr->chan_num);
	if(pkt->addr_block_present) {
		char *calling = fmt_x25_addr(pkt->calling.addr, pkt->calling.len);
		char *called = fmt_x25_addr(pkt->called.addr, pkt->called.len);
		la_vstring_append_sprintf(vstr, " src: %s dst: %s", calling, called);
		XFREE(calling);
		XFREE(called);
	} else if(pkt->type == X25_DATA) {
		la_vstring_append_sprintf(vstr, " sseq: %u rseq: %u more: %u",
			pkt->hdr->type.data.sseq, pkt->hdr->type.data.rseq, pkt->hdr->type.data.more);
	}
	la_vstring_append_sprintf(vstr, "%s", "\n");
	indent++;
	switch(pkt->type) {
	case X25_CALL_REQUEST:
	case X25_CALL_ACCEPTED:
		LA_ISPRINTF(vstr, indent, "%s", "Facilities:\n");
		tlv_format_as_text(vstr, pkt->facilities, x25_facility_names, indent+1);
		char *comp = fmt_bitfield(pkt->compression, x25_comp_algos);
		LA_ISPRINTF(vstr, indent, "Compression support: %s\n", comp);
		XFREE(comp);
		/* FALLTHROUGH */
		/* because Fast Select is on, so there might be a data PDU in call req or accept */
	case X25_DATA:
		break;
	case X25_CLEAR_REQUEST:
		LA_ISPRINTF(vstr, indent, "Cause: %02x\n", pkt->clr_cause);
		LA_ISPRINTF(vstr, indent, "Diagnostic code: %02x\n", pkt->diag_code);
		break;
	}
}

void x25_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	CAST_PTR(pkt, x25_pkt_t *, data);
	if(pkt->facilities != NULL) {
		tlv_list_free(pkt->facilities);
		pkt->facilities = NULL;
	}
	XFREE(data);
}

la_type_descriptor const proto_DEF_X25_pkt = {
	.format_text = x25_format_text,
	.destroy = x25_destroy
};
