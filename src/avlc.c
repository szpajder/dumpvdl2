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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <libacars/libacars.h>      // la_type_descriptor, la_proto_node
#include <libacars/vstring.h>       // la_vstring
#include <libacars/reassembly.h>    // la_reasm_ctx_new()
#include "config.h"                 // IS_BIG_ENDIAN
#include "dumpvdl2.h"
#include "avlc.h"
#include "ac_data.h"
#include "gs_data.h"
#include "xid.h"
#include "acars.h"
#include "x25.h"

#define MIN_AVLC_LEN    11
#define GOOD_FCS        0xF0B8u

#ifdef IS_BIG_ENDIAN
#define BSHIFT 0
#else
#define BSHIFT 24
#endif

// X.25 control field
typedef union {
	uint8_t val;
	struct {
#ifdef IS_BIG_ENDIAN
		uint8_t recv_seq:3;
		uint8_t poll:1;
		uint8_t send_seq:3;
		uint8_t type:1;
#else
		uint8_t type:1;
		uint8_t send_seq:3;
		uint8_t poll:1;
		uint8_t recv_seq:3;
#endif
	} I;
	struct {
#ifdef IS_BIG_ENDIAN
		uint8_t recv_seq:3;
		uint8_t pf:1;
		uint8_t sfunc:2;
		uint8_t type:2;
#else
		uint8_t type:2;
		uint8_t sfunc:2;
		uint8_t pf:1;
		uint8_t recv_seq:3;
#endif
	} S;
	struct {
#ifdef IS_BIG_ENDIAN
		uint8_t mfunc:6;
		uint8_t type:2;
#else
		uint8_t type:2;
		uint8_t mfunc:6;
#endif
	} U;
} lcf_t;

#define IS_I(lcf) (((lcf).val & 0x1) == 0x0)
#define IS_S(lcf) (((lcf).val & 0x3) == 0x1)
#define IS_U(lcf) (((lcf).val & 0x3) == 0x3)
#define U_MFUNC(lcf) ((lcf).U.mfunc & 0x3b)
#define U_PF(lcf) (((lcf).U.mfunc >> 2) & 0x1)

#define UI      0x00
#define DM      0x03
#define DISC    0x10
#define UA      0x18
#define FRMR    0x21
#define XID     0x2b
#define TEST    0x38

#define ADDRTYPE_AIRCRAFT   1
#define ADDRTYPE_GS_ADM     4
#define ADDRTYPE_GS_DEL     5
#define ADDRTYPE_ALL        7

#define IS_AIRCRAFT(addr) ((addr).a_addr.type == ADDRTYPE_AIRCRAFT)
#define IS_GS(addr) ((addr).a_addr.type == ADDRTYPE_GS_ADM || (addr).a_addr.type == ADDRTYPE_GS_DEL)

typedef struct {
	uint32_t num;
	avlc_addr_t src;
	avlc_addr_t dst;
	lcf_t lcf;
	avlc_frame_qentry_t *q;
} avlc_frame_t;

static const char *status_ag_descr[] = {
	"Airborne",
	"On ground"
};

static const char *status_cr_descr[] = {
	"Command",
	"Response"
};

static const char *addrtype_descr[] = {
	"reserved",
	"Aircraft",
	"reserved",
	"reserved",
	"Ground station",
	"Ground station",
	"reserved",
	"All stations"
};

static const char *S_cmd[] = {
	"Receive Ready",
	"Receive not Ready",
	"Reject",
	"Selective Reject"
};

static const char *U_cmd[] = {
	"UI",     "(0x01)", "(0x02)", "DM",     "(0x04)", "(0x05)", "(0x06)", "(0x07)",
	"(0x08)", "(0x09)", "(0x0a)", "(0x0b)", "(0x0c)", "(0x0d)", "(0x0e)", "(0x0f)",
	"DISC",   "(0x11)", "(0x12)", "(0x13)", "(0x14)", "(0x15)", "(0x16)", "(0x17)",
	"UA",     "(0x19)", "(0x1a)", "(0x1b)", "(0x1c)", "(0x1d)", "(0x1e)", "(0x1f)",
	"(0x20)", "FRMR",   "(0x22)", "(0x23)", "(0x24)", "(0x25)", "(0x26)", "(0x27)",
	"(0x28)", "(0x29)", "(0x2a)", "XID",    "(0x2c)", "(0x2d)", "(0x2e)", "(0x2f)",
	"(0x30)", "(0x31)", "(0x32)", "(0x33)", "(0x34)", "(0x35)", "(0x36)", "(0x37)",
	"TEST"
};

// Forward declaration
la_type_descriptor const proto_DEF_avlc_frame;

uint32_t parse_dlc_addr(uint8_t *buf) {
	debug_print(D_PROTO_DETAIL, "%02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3]);
	return reverse((buf[0] >> 1) | (buf[1] << 6) | (buf[2] << 13) | ((buf[3] & 0xfe) << 20), 28) & ONES(28);
}

la_proto_node *avlc_parse(avlc_frame_qentry_t *q, uint32_t *msg_type, la_reasm_ctx *reasm_ctx) {
	ASSERT(q != NULL);
	uint8_t *buf = q->buf;
	uint32_t len = q->len;
	if(len < MIN_AVLC_LEN) {
		debug_print(D_PROTO, "Frame %d: too short (len=%u required=%d)\n", q->metadata->idx, len, MIN_AVLC_LEN);
		statsd_increment_per_channel(q->metadata->freq, "avlc.errors.too_short");
		return NULL;
	}
	debug_print(D_PROTO, "Frame %d: len=%u\n", q->metadata->idx, len);
	debug_print_buf_hex(D_PROTO_DETAIL, buf, len, "Frame data:\n");

	// FCS check
	uint16_t fcs = crc16_ccitt(buf, len, 0xFFFFu);
	debug_print(D_PROTO_DETAIL, "Check FCS: %04x\n", fcs);
	if(fcs == GOOD_FCS) {
		debug_print(D_PROTO, "FCS check OK\n");
		statsd_increment_per_channel(q->metadata->freq, "avlc.frames.good");
		len -= 2;
	} else {
		debug_print(D_PROTO, "FCS check failed\n");
		statsd_increment_per_channel(q->metadata->freq, "avlc.errors.bad_fcs");
		return NULL;
	}

	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_avlc_frame;
	NEW(avlc_frame_t, frame);
	node->data = frame;
	node->next = NULL;
	frame->q = q;

	uint8_t *ptr = buf;
	frame->num = q->metadata->idx;
	frame->dst.val = parse_dlc_addr(ptr);
	ptr += 4; len -= 4;
	frame->src.val = parse_dlc_addr(ptr);
	ptr += 4; len -= 4;

	switch(frame->src.a_addr.type) {
		case ADDRTYPE_AIRCRAFT:
			*msg_type |= MSGFLT_SRC_AIR;
#ifdef WITH_STATSD
			switch(frame->dst.a_addr.type) {
				case ADDRTYPE_GS_ADM:
				case ADDRTYPE_GS_DEL:
					statsd_increment_per_channel(q->metadata->freq, "avlc.msg.air2gnd");
					break;
				case ADDRTYPE_AIRCRAFT:
					statsd_increment_per_channel(q->metadata->freq, "avlc.msg.air2air");
					break;
				case ADDRTYPE_ALL:
					statsd_increment_per_channel(q->metadata->freq, "avlc.msg.air2all");
					break;
			}
#endif
			break;
		case ADDRTYPE_GS_ADM:
		case ADDRTYPE_GS_DEL:
			*msg_type |= MSGFLT_SRC_GND;
#ifdef WITH_STATSD
			switch(frame->dst.a_addr.type) {
				case ADDRTYPE_AIRCRAFT:
					statsd_increment_per_channel(q->metadata->freq, "avlc.msg.gnd2air");
					break;
				case ADDRTYPE_GS_ADM:
				case ADDRTYPE_GS_DEL:
					statsd_increment_per_channel(q->metadata->freq, "avlc.msg.gnd2gnd");
					break;
				case ADDRTYPE_ALL:
					statsd_increment_per_channel(q->metadata->freq, "avlc.msg.gnd2all");
					break;
			}
#endif
			break;
	}

	frame->lcf.val = *ptr++;
	len--;
	if(IS_S(frame->lcf)) {
		*msg_type |= MSGFLT_AVLC_S;
		/* TODO */
		node->next = unknown_proto_pdu_new(ptr, len);
	} else if(IS_U(frame->lcf)) {
		*msg_type |= MSGFLT_AVLC_U;
		if(U_MFUNC(frame->lcf) == XID) {
			node->next = xid_parse(frame->src.a_addr.status, U_PF(frame->lcf), ptr, len, msg_type);
		} else {
			node->next = unknown_proto_pdu_new(ptr, len);
		}
	} else {     // IS_I(frame->lcf) == true
		*msg_type |= MSGFLT_AVLC_I;
		if(len > 3 && ptr[0] == 0xff && ptr[1] == 0xff && ptr[2] == 0x01) {
			node->next = parse_acars(ptr + 3, len - 3, msg_type, reasm_ctx, q->metadata->burst_timestamp);
		} else {
			node->next = x25_parse(ptr, len, msg_type, reasm_ctx, q->metadata->burst_timestamp,
					frame->src.a_addr.addr, frame->dst.a_addr.addr);
		}
	}
	return node;
}

static void addrinfo_format_as_text(la_vstring *vstr, int indent, avlc_addr_t const addr) {
	if(IS_AIRCRAFT(addr)) {
		if(Config.ac_addrinfo_db_available == true) {
			ac_data_entry *ac = ac_data_entry_lookup(addr.a_addr.addr);
			if(Config.addrinfo_verbosity == ADDRINFO_TERSE) {
				la_vstring_append_sprintf(vstr, " [%s]",
						ac && ac->registration ? ac->registration : "-"
						);
			} else if(Config.addrinfo_verbosity == ADDRINFO_NORMAL) {
				LA_ISPRINTF(vstr, indent, "AC info: %s, %s, %s\n",
						ac && ac->registration ? ac->registration : "-",
						ac && ac->icaotypecode ? ac->icaotypecode : "-",
						ac && ac->operatorflagcode ? ac->operatorflagcode : "-"
						);
			} else if(Config.addrinfo_verbosity == ADDRINFO_VERBOSE) {
				LA_ISPRINTF(vstr, indent, "AC info: %s, %s, %s, %s\n",
						ac && ac->registration ? ac->registration : "-",
						ac && ac->manufacturer ? ac->manufacturer : "-",
						ac && ac->type ? ac->type : "-",
						ac && ac->registeredowners ? ac->registeredowners : "-"
						);
			}
		}
	} else if(IS_GS(addr)) {
		if(Config.gs_addrinfo_db_available == true) {
			gs_data_entry *gs = gs_data_entry_lookup(addr.a_addr.addr);
			if(Config.addrinfo_verbosity == ADDRINFO_TERSE) {
				la_vstring_append_sprintf(vstr, " [%s]",
						gs && gs->airport_code ? gs->airport_code : "-"
						);
			} else if(Config.addrinfo_verbosity == ADDRINFO_NORMAL) {
				LA_ISPRINTF(vstr, indent, "GS info: %s, %s\n",
						gs && gs->airport_code ? gs->airport_code : "-",
						gs && gs->location ? gs->location : "-"
						);
			} else if(Config.addrinfo_verbosity == ADDRINFO_VERBOSE) {
				LA_ISPRINTF(vstr, indent, "GS info: %s\n",
						gs && gs->details ? gs->details : "-"
						);
			}
		}
	}
}

void avlc_format_text(la_vstring * const vstr, void const * const data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	CAST_PTR(f, avlc_frame_t *, data);

	if(Config.output_raw_frames == true && f->q->len > 0) {
		append_hexdump_with_indent(vstr, f->q->buf, f->q->len, indent+1);
	}

	LA_ISPRINTF(vstr, indent, "%06X (%s, %s)",
			f->src.a_addr.addr,
			addrtype_descr[f->src.a_addr.type],
			status_ag_descr[f->dst.a_addr.status]   // A/G
			);
	// Print extra info about source and/or destination?
	// TERSE verbosity level is printed inline.
	if(Config.addrinfo_verbosity == ADDRINFO_TERSE) {
		addrinfo_format_as_text(vstr, indent, f->src);
	}

	la_vstring_append_sprintf(vstr, " -> %06X (%s)",
			f->dst.a_addr.addr,
			addrtype_descr[f->dst.a_addr.type]
			);
	if(Config.addrinfo_verbosity == ADDRINFO_TERSE) {
		addrinfo_format_as_text(vstr, indent, f->dst);
	}
	la_vstring_append_sprintf(vstr, ": %s\n",
			status_cr_descr[f->src.a_addr.status]   // C/R
			);

	// Print extra info about source and/or destination?
	// Verbosity levels above TERSE are printed as separate lines.
	if(Config.addrinfo_verbosity > ADDRINFO_TERSE) {
		addrinfo_format_as_text(vstr, indent, f->src);
		addrinfo_format_as_text(vstr, indent, f->dst);
	}

	if(IS_S(f->lcf)) {
		LA_ISPRINTF(vstr, indent, "AVLC type: S (%s) P/F: %x rseq: %x\n",
				S_cmd[f->lcf.S.sfunc], f->lcf.S.pf, f->lcf.S.recv_seq);
	} else if(IS_U(f->lcf)) {
		LA_ISPRINTF(vstr, indent, "AVLC type: U (%s) P/F: %x\n",
				U_cmd[U_MFUNC(f->lcf)], U_PF(f->lcf));
	} else {    // IS_I == true
		LA_ISPRINTF(vstr, indent, "AVLC type: I sseq: %x rseq: %x poll: %x\n",
				f->lcf.I.send_seq, f->lcf.I.recv_seq, f->lcf.I.poll);
	}
}

la_type_descriptor const proto_DEF_avlc_frame = {
	.format_text = avlc_format_text,
	.destroy = NULL
};
