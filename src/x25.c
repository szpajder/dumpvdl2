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

static void *parse_x25_user_data(x25_pkt_t *pkt, uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	if(buf == NULL || len == 0)
		return NULL;
	uint8_t proto = *buf;
	if(proto == SN_PROTO_CLNP) {
		pkt->proto = SN_PROTO_CLNP;
		return parse_clnp_pdu(buf, len, msg_type);
	} else if(proto == SN_PROTO_ESIS) {
		pkt->proto = SN_PROTO_ESIS;
		return parse_esis_pdu(buf, len, msg_type);
	}
	uint8_t pdu_type = proto >> 4;
	if(pdu_type < 4) {
		pkt->proto = SN_PROTO_CLNP_INIT_COMPRESSED;
		return parse_clnp_compressed_init_pdu(buf, len, msg_type);
	}
	pkt->proto = proto;
	return NULL;
}

x25_pkt_t *parse_x25(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	static x25_pkt_t *pkt = NULL;
	int ret;

	if(len < X25_MIN_LEN) {
		debug_print("Too short (len %u < min len %u)\n", len, X25_MIN_LEN);
		return NULL;
	}

	x25_hdr_t *hdr = (x25_hdr_t *)buf;
	debug_print("gfi=0x%02x group=0x%02x chan=0x%02x type=0x%02x\n", hdr->gfi, hdr->chan_group, hdr->chan_num, hdr->type.val);
	if(hdr->gfi != GFI_X25_MOD8) {
		debug_print("Unsupported GFI 0x%x\n", hdr->gfi);
		return NULL;
	}
	if(pkt == NULL) {
		pkt = XCALLOC(1, sizeof(x25_pkt_t));
	} else {
		if(pkt->facilities != NULL)
			tlv_list_free(pkt->facilities);
		memset(pkt, 0, sizeof(x25_pkt_t));
	}

	uint8_t *ptr = buf + sizeof(x25_hdr_t);
	len -= sizeof(x25_hdr_t);

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
	switch(pkt->type) {
	case X25_CALL_REQUEST:
	case X25_CALL_ACCEPTED:
		if((ret = parse_x25_address_block(pkt, ptr, len)) < 0)
			return NULL;
		ptr += ret; len -= ret;
		if((ret = parse_x25_facility_field(pkt, ptr, len)) < 0)
			return NULL;
		ptr += ret; len -= ret;
		if(pkt->type == X25_CALL_REQUEST) {
			if((ret = parse_x25_callreq_sndcf(pkt, ptr, len)) < 0)
				return NULL;
			ptr += ret; len -= ret;
		} else if(pkt->type == X25_CALL_ACCEPTED) {
			if(len > 0) {
				pkt->compression = *ptr++;
				len--;
			} else {
				debug_print("%s", "X25_CALL_ACCEPT: no payload\n");
				return NULL;
			}
		}
	/* FALLTHROUGH because Fast Select is on, so there might be a data PDU in call req or accept */
	case X25_DATA:
		pkt->data = parse_x25_user_data(pkt, ptr, len, msg_type);
		break;
	case X25_CLEAR_REQUEST:
		if(len > 0) {
			pkt->clr_cause = *ptr++;
			len--;
		}
		if(len > 0) {
			pkt->diag_code = *ptr++;
			len--;
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
		return NULL;
	}
	pkt->hdr = hdr;
	if(pkt->data == NULL) {		// unparsed payload
		pkt->data = ptr;
		pkt->datalen = len;
		pkt->data_valid = 0;
	} else {
		pkt->data_valid = 1;
	}
	return pkt;
}

void output_x25(x25_pkt_t *pkt) {
	char *name = (char *)dict_search(x25_pkttype_names, pkt->type);
	fprintf(outf, "X.25 %s: grp: %u chan: %u", name, pkt->hdr->chan_group, pkt->hdr->chan_num);
	if(pkt->addr_block_present) {
		fprintf(outf, " src: %s dst: %s",
			fmt_x25_addr(pkt->calling.addr, pkt->calling.len),
			fmt_x25_addr(pkt->called.addr, pkt->called.len)
		);
	} else if(pkt->type == X25_DATA) {
		fprintf(outf, " sseq: %u rseq: %u more: %u",
			pkt->hdr->type.data.sseq, pkt->hdr->type.data.rseq, pkt->hdr->type.data.more);
	}
	fprintf(outf, "\n");
	switch(pkt->type) {
	case X25_CALL_REQUEST:
	case X25_CALL_ACCEPTED:
		fprintf(outf, "Facilities:\n");
		output_tlv(outf, pkt->facilities, x25_facility_names);
		char *comp = fmt_bitfield(pkt->compression, x25_comp_algos);
		fprintf(outf, "Compression support: %s\n", comp);
		XFREE(comp);
		/* FALLTHROUGH because Fast Select is on, so there might be a data PDU in call req or accept */
	case X25_DATA:
		switch(pkt->proto) {
		case SN_PROTO_CLNP_INIT_COMPRESSED:
			if(pkt->data_valid) {
				output_clnp_compressed(pkt->data);
			} else {
				fprintf(outf, "-- Unparseable CLNP PDU\n");
				output_raw(pkt->data, pkt->datalen);
			}
			break;
		case SN_PROTO_CLNP:
			if(pkt->data_valid) {
				output_clnp(pkt->data);
			} else {
				fprintf(outf, "-- Unparseable CLNP PDU\n");
				output_raw(pkt->data, pkt->datalen);
			}
			break;
		case SN_PROTO_ESIS:
			if(pkt->data_valid) {
				output_esis(pkt->data);
			} else {
				fprintf(outf, "-- Unparseable ES-IS PDU\n");
				output_raw(pkt->data, pkt->datalen);
			}
			break;
		case SN_PROTO_IDRP:
			fprintf(outf, "IDRP PDU:\n");
			output_raw(pkt->data, pkt->datalen);
			break;
		default:
			fprintf(outf, "Unknown protocol 0x%02x PDU:\n", pkt->proto);
			break;
		}
		break;
	case X25_CLEAR_REQUEST:
		fprintf(outf, " Cause: %02x\n Diagnostic code: %02x\n", pkt->clr_cause, pkt->diag_code);
		break;
	}
}
