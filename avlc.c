/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
 *  Copyright (c) 2017 Tomasz Lemiech <szpajder@gmail.com>
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
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include "dumpvdl2.h"
#include "avlc.h"
#include "xid.h"
#include "acars.h"
#include "x25.h"

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
	"(0x18)", "(0x19)", "(0x1a)", "(0x1b)", "(0x1c)", "(0x1d)", "(0x1e)", "(0x1f)",
	"(0x20)", "FRMR",   "(0x22)", "(0x23)", "(0x24)", "(0x25)", "(0x26)", "(0x27)",
	"(0x28)", "(0x29)", "(0x2a)", "XID",    "(0x2c)", "(0x2d)", "(0x2e)", "(0x2f)",
	"(0x30)", "(0x31)", "(0x32)", "(0x33)", "(0x34)", "(0x35)", "(0x36)", "(0x37)",
	"TEST"
};

uint32_t parse_dlc_addr(uint8_t *buf) {
	debug_print("%02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3]);
	return reverse((buf[0] >> 1) | (buf[1] << 6) | (buf[2] << 13) | ((buf[3] & 0xfe) << 20), 28) & ONES(28);
}

static void parse_avlc(vdl2_channel_t *v, uint8_t *buf, uint32_t len) {
	debug_print_buf_hex(buf, len, "%s", "Frame data:\n");
// FCS check
	len -= 2;
	uint16_t read_fcs = ((uint16_t)buf[len+1] << 8) | buf[len];
	uint16_t fcs = crc16_ccitt(buf, len);
	debug_print("Read FCS : %04x\n", read_fcs);
	debug_print("Check FCS: %04x\n", fcs);
	if(read_fcs == fcs) {
		debug_print("%s", "FCS check OK\n");
	} else {
		debug_print("%s", "FCS check failed\n");
		statsd_increment(v->freq, "avlc.errors.bad_fcs");
		return;
	}
	statsd_increment(v->freq, "avlc.frames.good");
	uint8_t *ptr = buf;
	avlc_frame_t frame;
	uint32_t msg_type = 0;
	frame.t = time(NULL);
	frame.dst.val = parse_dlc_addr(ptr);
	ptr += 4; len -= 4;
	frame.src.val = parse_dlc_addr(ptr);
	ptr += 4; len -= 4;

	switch(frame.src.a_addr.type) {
	case ADDRTYPE_AIRCRAFT:
		msg_type |= MSGFLT_SRC_AIR;
#if USE_STATSD
		switch(frame.dst.a_addr.type) {
		case ADDRTYPE_GS_ADM:
		case ADDRTYPE_GS_DEL:
			statsd_increment(v->freq, "avlc.msg.air2gnd");
			break;
		case ADDRTYPE_ALL:
			statsd_increment(v->freq, "avlc.msg.air2all");
			break;
		}
#endif
		break;
	case ADDRTYPE_GS_ADM:
	case ADDRTYPE_GS_DEL:
		msg_type |= MSGFLT_SRC_GND;
#if USE_STATSD
		switch(frame.dst.a_addr.type) {
		case ADDRTYPE_AIRCRAFT:
			statsd_increment(v->freq, "avlc.msg.gnd2air");
			break;
		case ADDRTYPE_GS_ADM:
		case ADDRTYPE_GS_DEL:
			statsd_increment(v->freq, "avlc.msg.gnd2gnd");
			break;
		case ADDRTYPE_ALL:
			statsd_increment(v->freq, "avlc.msg.gnd2all");
			break;
		}
#endif
		break;
	}

	frame.lcf.val = *ptr++;
	len--;
	frame.data_valid = 0;
	frame.data = NULL;
	if(IS_S(frame.lcf)) {
		msg_type |= MSGFLT_AVLC_S;
		/* TODO */
	} else if(IS_U(frame.lcf)) {
		msg_type |= MSGFLT_AVLC_U;
		switch(U_MFUNC(frame.lcf)) {
		case XID:
			frame.data = parse_xid(frame.src.a_addr.status, U_PF(frame.lcf), ptr, len, &msg_type);
			break;
		}
	} else { 	// IS_I(frame.lcf) == true
		msg_type |= MSGFLT_AVLC_I;
		if(len > 3 && ptr[0] == 0xff && ptr[1] == 0xff && ptr[2] == 0x01) {
			frame.proto = PROTO_ACARS;
			frame.data = parse_acars(ptr + 3, len - 3, &msg_type);
		} else {
			frame.proto = PROTO_X25;
			frame.data = parse_x25(ptr, len, &msg_type);
		}
	}
	if(frame.data == NULL) {	// unparseable frame
		frame.data = ptr;
		frame.datalen = len;
	} else {
		frame.data_valid = 1;
	}
	if((msg_type & msg_filter) == msg_type) {
		debug_print("msg_type: %x msg_filter: %x (accepted)\n", msg_type, msg_filter);
		output_avlc(v, &frame);
	} else {
		debug_print("msg_type: %x msg_filter: %x (filtered out)\n", msg_type, msg_filter);
	}
}

void parse_avlc_frames(vdl2_channel_t *v, uint8_t *buf, uint32_t len) {
	if(buf[0] != AVLC_FLAG) {
		debug_print("%s", "No AVLC frame delimiter at the start\n");
		statsd_increment(v->freq, "avlc.errors.no_flag_start");
		return;
	}
	uint32_t fcnt = 0, goodfcnt = 0;
	uint8_t *frame_start = buf + 1;
	uint8_t *frame_end;
	uint8_t *buf_end = buf + len;
	uint32_t flen;
	while(frame_start < buf_end - 1) {
		statsd_increment(v->freq, "avlc.frames.processed");
		if((frame_end = memchr(frame_start, AVLC_FLAG, buf_end - frame_start)) == NULL) {
			debug_print("Frame %u: truncated\n", fcnt);
			statsd_increment(v->freq, "avlc.errors.no_flag_end");
			return;
		}
		flen = frame_end - frame_start;
		if(flen < MIN_AVLC_LEN) {
			debug_print("Frame %u: too short (len=%u required=%d)\n", fcnt, flen, MIN_AVLC_LEN);
			statsd_increment(v->freq, "avlc.errors.too_short");
			goto next;
		}
		debug_print("Frame %u: len=%u\n", fcnt, flen);
		goodfcnt++;
		parse_avlc(v, frame_start, flen);
next:
		frame_start = frame_end + 1;
		fcnt++;
	}
	debug_print("%u/%u frames processed\n", goodfcnt, fcnt);
}

static void output_avlc_U(const avlc_frame_t *f) {
	switch(U_MFUNC(f->lcf)) {
	case XID:
		if(f->data_valid)
			output_xid((xid_msg_t *)f->data);
		else {
			fprintf(outf, "-- Unparseable XID\n");
			output_raw((uint8_t *)f->data, f->datalen);
		}
		break;
	default:
		output_raw((uint8_t *)f->data, f->datalen);
	}
}

void output_avlc(vdl2_channel_t *v, const avlc_frame_t *f) {
	if(f == NULL) return;
	if((daily || hourly) && rotate_outfile() < 0)
		_exit(1);
	char ftime[24];
	strftime(ftime, sizeof(ftime), "%F %T %Z", (utc ? gmtime(&f->t) : localtime(&f->t)));
	float sig_pwr_dbfs = 20.0f * log10f(v->mag_frame);
	float nf_pwr_dbfs = 20.0f * log10f(v->mag_nf + 0.001f);
	fprintf(outf, "\n[%s] [%.3f] [%.1f/%.1f dBFS] [%.1f dB]\n",
		ftime, (float)v->freq / 1e+6, sig_pwr_dbfs, nf_pwr_dbfs, sig_pwr_dbfs-nf_pwr_dbfs);
	fprintf(outf, "%06X (%s, %s) -> %06X (%s): %s\n",
		f->src.a_addr.addr,
		addrtype_descr[f->src.a_addr.type],
		status_ag_descr[f->dst.a_addr.status],	// A/G
		f->dst.a_addr.addr,
		addrtype_descr[f->dst.a_addr.type],
		status_cr_descr[f->src.a_addr.status]	// C/R
	);
	if(IS_S(f->lcf)) {
		fprintf(outf, "AVLC: type: S (%s) P/F: %x rseq: %x\n", S_cmd[f->lcf.S.sfunc], f->lcf.S.pf, f->lcf.S.recv_seq);
		output_raw((uint8_t *)f->data, f->datalen);
	} else if(IS_U(f->lcf)) {
		fprintf(outf, "AVLC: type: U (%s) P/F: %x\n", U_cmd[U_MFUNC(f->lcf)], U_PF(f->lcf));
		output_avlc_U(f);
	} else {	// IS_I == true
		fprintf(outf, "AVLC type: I sseq: %x rseq: %x poll: %x\n", f->lcf.I.send_seq, f->lcf.I.recv_seq, f->lcf.I.poll);
		switch(f->proto) {
		case PROTO_ACARS:
			if(f->data_valid)
				output_acars((acars_msg_t *)f->data);
			else {
				fprintf(outf, "-- Unparseable ACARS payload\n");
				output_raw((uint8_t *)f->data, f->datalen);
			}
			break;
		case PROTO_X25:
			if(f->data_valid)
				output_x25((x25_pkt_t *)f->data);
			else {
				fprintf(outf, "-- Unparseable X.25 packet\n");
				output_raw((uint8_t *)f->data, f->datalen);
			}
			break;
		default:
			output_raw((uint8_t *)f->data, f->datalen);
		}
	}
	fflush(outf);
}
