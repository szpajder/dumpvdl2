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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>		// strftime, gmtime, localtime
#include <math.h>
#include <unistd.h>
#include <glib.h>
#include <libacars/libacars.h>	// la_type_descriptor, la_proto_node
#include <libacars/vstring.h>	// la_vstring
#include "config.h"		// IS_BIG_ENDIAN
#include "dumpvdl2.h"
#include "avlc.h"
#include "xid.h"
#include "acars.h"
#include "x25.h"

#define MIN_AVLC_LEN	11
#define GOOD_FCS	0xF0B8u

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

#define UI	0x00
#define DM	0x03
#define DISC	0x10
#define UA	0x18
#define FRMR	0x21
#define XID	0x2b
#define TEST	0x38

#define ADDRTYPE_AIRCRAFT	1
#define ADDRTYPE_GS_ADM		4
#define ADDRTYPE_GS_DEL		5
#define ADDRTYPE_ALL		7

enum avlc_protocols { PROTO_X25, PROTO_ACARS, PROTO_UNKNOWN };
typedef struct {
	uint32_t num;
	avlc_addr_t src;
	avlc_addr_t dst;
	lcf_t lcf;
	enum avlc_protocols proto;
	uint32_t datalen;
	uint8_t data_valid;
	void *data;
// TEMP
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
	debug_print("%02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3]);
	return reverse((buf[0] >> 1) | (buf[1] << 6) | (buf[2] << 13) | ((buf[3] & 0xfe) << 20), 28) & ONES(28);
}

static la_proto_node *parse_avlc(avlc_frame_qentry_t *q, uint32_t *msg_type) {
	uint8_t *buf = q->buf;
	uint32_t len = q->len;
	debug_print_buf_hex(buf, len, "%s", "Frame data:\n");

// FCS check
	uint16_t fcs = crc16_ccitt(buf, len, 0xFFFFu);
	debug_print("Check FCS: %04x\n", fcs);
	if(fcs == GOOD_FCS) {
		debug_print("%s", "FCS check OK\n");
		statsd_increment(q->freq, "avlc.frames.good");
		len -= 2;
	} else {
		debug_print("%s", "FCS check failed\n");
		statsd_increment(q->freq, "avlc.errors.bad_fcs");
		return NULL;
	}

	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_avlc_frame;
	avlc_frame_t *frame = XCALLOC(1, sizeof(avlc_frame_t));
	node->data = frame;
	node->next = NULL;
// FIXME: make a copy
	frame->q = q;

	uint8_t *ptr = buf;
	frame->num = q->idx;
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
			statsd_increment(q->freq, "avlc.msg.air2gnd");
			break;
		case ADDRTYPE_AIRCRAFT:
			statsd_increment(q->freq, "avlc.msg.air2air");
			break;
		case ADDRTYPE_ALL:
			statsd_increment(q->freq, "avlc.msg.air2all");
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
			statsd_increment(q->freq, "avlc.msg.gnd2air");
			break;
		case ADDRTYPE_GS_ADM:
		case ADDRTYPE_GS_DEL:
			statsd_increment(q->freq, "avlc.msg.gnd2gnd");
			break;
		case ADDRTYPE_ALL:
			statsd_increment(q->freq, "avlc.msg.gnd2all");
			break;
		}
#endif
		break;
	}

	frame->lcf.val = *ptr++;
	len--;
	frame->data_valid = 0;
	frame->data = NULL;
	if(IS_S(frame->lcf)) {
		*msg_type |= MSGFLT_AVLC_S;
		/* TODO */
	} else if(IS_U(frame->lcf)) {
		*msg_type |= MSGFLT_AVLC_U;
		if(U_MFUNC(frame->lcf) == XID) {
			node->next = xid_parse(frame->src.a_addr.status, U_PF(frame->lcf), ptr, len, msg_type);
		}
	} else { 	// IS_I(frame->lcf) == true
		*msg_type |= MSGFLT_AVLC_I;
		if(len > 3 && ptr[0] == 0xff && ptr[1] == 0xff && ptr[2] == 0x01) {
			frame->proto = PROTO_ACARS;
			frame->data = NULL;
			node->next = parse_acars(ptr + 3, len - 3, msg_type);
		} else {
			frame->proto = PROTO_X25;
			frame->data = NULL;
			node->next = x25_parse(ptr, len, msg_type);
		}
	}
	if(frame->data == NULL) {	// unparseable frame
		frame->data = ptr;
		frame->datalen = len;
	} else {
		frame->data_valid = 1;
	}
	return node;
}

GAsyncQueue *frame_queue = NULL;

void *parse_avlc_frames(void *arg) {
// -Wunused-parameter
	(void)arg;
	avlc_frame_qentry_t *q = NULL;
	la_proto_node *root = NULL;
	uint32_t msg_type = 0;

	frame_queue = g_async_queue_new();
	while(1) {
		q = (avlc_frame_qentry_t *)g_async_queue_pop(frame_queue);
		statsd_increment(q->freq, "avlc.frames.processed");
		if(q->len < MIN_AVLC_LEN) {
			debug_print("Frame %d: too short (len=%u required=%d)\n", q->idx, q->len, MIN_AVLC_LEN);
			statsd_increment(q->freq, "avlc.errors.too_short");
			goto cleanup;
		}
		debug_print("Frame %d: len=%u\n", q->idx, q->len);
		msg_type = 0;
		root = parse_avlc(q, &msg_type);
		if(root == NULL) {
			goto cleanup;
		}
		if((msg_type & msg_filter) == msg_type) {
			debug_print("msg_type: %x msg_filter: %x (accepted)\n", msg_type, msg_filter);
			output_proto_tree(root);
		} else {
			debug_print("msg_type: %x msg_filter: %x (filtered out)\n", msg_type, msg_filter);
		}
cleanup:
		la_proto_tree_destroy(root);
		root = NULL;
		XFREE(q->buf);
		XFREE(q);
	}
}

void avlc_format_text(la_vstring * const vstr, void const * const data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	CAST_PTR(f, avlc_frame_t *, data);

	char ftime[30];
	strftime(ftime, sizeof(ftime), "%F %T %Z",
		(utc ? gmtime(&f->q->burst_timestamp.tv_sec) : localtime(&f->q->burst_timestamp.tv_sec)));
	float sig_pwr_dbfs = 10.0f * log10f(f->q->frame_pwr);
	float nf_pwr_dbfs = 20.0f * log10f(f->q->mag_nf + 0.001f);
	LA_ISPRINTF(vstr, indent, "[%s] [%.3f] [%.1f/%.1f dBFS] [%.1f dB] [%.1f ppm]",
		ftime, (float)f->q->freq / 1e+6, sig_pwr_dbfs, nf_pwr_dbfs, sig_pwr_dbfs-nf_pwr_dbfs,
		f->q->ppm_error);

	if(extended_header) {
		la_vstring_append_sprintf(vstr, " [S:%d] [L:%u] [F:%d] [#%u]",
			 f->q->synd_weight, f->q->datalen_octets, f->q->num_fec_corrections, f->num);
	}
	la_vstring_append_sprintf(vstr, "%s", "\n");

	if(output_raw_frames && f->q->len > 0) {
		append_hexdump_with_indent(vstr, f->q->buf, f->q->len, indent+1);
	}

	LA_ISPRINTF(vstr, indent, "%06X (%s, %s) -> %06X (%s): %s\n",
		f->src.a_addr.addr,
		addrtype_descr[f->src.a_addr.type],
		status_ag_descr[f->dst.a_addr.status],	// A/G
		f->dst.a_addr.addr,
		addrtype_descr[f->dst.a_addr.type],
		status_cr_descr[f->src.a_addr.status]	// C/R
	);
	if(IS_S(f->lcf)) {
		LA_ISPRINTF(vstr, indent, "AVLC type: S (%s) P/F: %x rseq: %x\n",
			S_cmd[f->lcf.S.sfunc], f->lcf.S.pf, f->lcf.S.recv_seq);
		if(f->datalen > 0) {
			append_hexstring_with_indent(vstr, (uint8_t *)f->data, f->datalen, indent+1);
		}
	} else if(IS_U(f->lcf)) {
		LA_ISPRINTF(vstr, indent, "AVLC type: U (%s) P/F: %x\n", U_cmd[U_MFUNC(f->lcf)], U_PF(f->lcf));
		switch(U_MFUNC(f->lcf)) {
		case XID:
			break;
		default:
			if(f->datalen > 0) {
				append_hexstring_with_indent(vstr, (uint8_t *)f->data, f->datalen, indent+1);
			}
		}
	} else {	// IS_I == true
		LA_ISPRINTF(vstr, indent, "AVLC type: I sseq: %x rseq: %x poll: %x\n", f->lcf.I.send_seq, f->lcf.I.recv_seq, f->lcf.I.poll);
		switch(f->proto) {
		case PROTO_ACARS:
		case PROTO_X25:
			break;
		default:
//			append_hexdump_with_indent(vstr, f->q->buf, f->q->len, indent+1);
			append_hexstring_with_indent(vstr, (uint8_t *)f->data, f->datalen, indent+1);
		}
	}
}

la_type_descriptor const proto_DEF_avlc_frame = {
	.format_text = avlc_format_text,
	.destroy = NULL
};
