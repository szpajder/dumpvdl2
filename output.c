#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include "dumpvdl2.h"
#include "avlc.h"
#include "acars.h"
#include "xid.h"
#include "x25.h"

static const char *status_ag_descr[] = {
	"Airborne",
	"On ground"
};

static const char *status_cr_descr[] = {
	"Command frame",
	"Response frame"
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

FILE *outf;
uint8_t hourly = 0, daily = 0;
static char *filename_prefix = NULL;
static size_t prefix_len;
static struct tm current_tm;

static int open_outfile() {
	assert(filename_prefix);
	char *filename, *fmt;
	size_t tlen;

	if(hourly || daily) {
		time_t t = time(NULL);
		localtime_r(&t, &current_tm);
		char suffix[16];
		if(hourly)
			fmt = "_%Y%m%d_%H";
		else	// daily
			fmt = "_%Y%m%d";
		tlen = strftime(suffix, sizeof(suffix), fmt, &current_tm);
		if(tlen == 0) {
			fprintf(stderr, "open_outfile(): strfime returned 0\n");
			return -1;
		}
		filename = XCALLOC(prefix_len + tlen + 2, sizeof(uint8_t));
		sprintf(filename, "%s%s", filename_prefix, suffix);
	} else {
		filename = filename_prefix;
	}

	if((outf = fopen(filename, "a+")) == NULL) {
		fprintf(stderr, "Could not open output file %s: %s\n", filename, strerror(errno));
		return -1;
	}
	if(hourly || daily)
		free(filename);

	return 0;
}

int init_output_file(char *file) {
	assert(file);
	if(!strcmp(file, "-")) {
		outf = stdout;
	} else {
		filename_prefix = file;
		prefix_len = strlen(filename_prefix);
		return open_outfile();
	}
	return 0;
}

static int rotate_outfile() {
	struct tm new_tm;
	time_t t = time(NULL);
	localtime_r(&t, &new_tm);
	if((hourly && new_tm.tm_hour != current_tm.tm_hour) || (daily && new_tm.tm_mday != current_tm.tm_mday)) {
		fclose(outf);
		return open_outfile();
	}
	return 0;
}

static void output_acars(const acars_msg_t *msg) {
	assert(msg);
	fprintf(outf, "ACARS:\n");
	if(msg->mode < 0x5d)
		fprintf(outf, "Aircraft reg: %s Flight: %s\n", msg->reg, msg->fid);
	fprintf(outf, "Mode: %1c Label: %s Blk id: %c Ack: %c Msg no.: %s\n",
		msg->mode, msg->label, msg->bid, msg->ack, msg->no);
	fprintf(outf, "Message:\n%s\n", msg->txt);
}

void output_raw(uint8_t *buf, uint32_t len) {
	if(len == 0)
		return;
	fprintf(outf, "   ");
	for(int i = 0; i < len; i++)
		fprintf(outf, "%02x ", buf[i]);
	fprintf(outf, "\n");
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

void output_avlc(vdl2_state_t *v, const avlc_frame_t *f) {
	if(f == NULL) return;
	if((daily || hourly) && rotate_outfile() < 0)
		_exit(1);
	char ftime[24];
	strftime(ftime, sizeof(ftime), "%F %T", localtime(&f->t));
	float snr = 20.0f * log10f(v->mag_frame / (v->mag_nf + 0.001f));
	fprintf(outf, "\n[%s] [%.2f:%.2f] [%.1f dB]\n", ftime, v->mag_frame, v->mag_nf, snr);
	fprintf(outf, "%06X (%s, %s) -> %06X (%s): %s, CF: 0x%02x\n",
		f->src.a_addr.addr,
		addrtype_descr[f->src.a_addr.type],
		status_ag_descr[f->dst.a_addr.status],	// A/G
		f->dst.a_addr.addr,
		addrtype_descr[f->dst.a_addr.type],
		status_cr_descr[f->src.a_addr.status],	// C/R
		f->lcf.val
	);
	if(IS_S(f->lcf)) {
		fprintf(outf, "S: sfunc=0x%x (%s) P/F=%x rseq=0x%x\n", f->lcf.S.sfunc, S_cmd[f->lcf.S.sfunc], f->lcf.S.pf, f->lcf.S.recv_seq);
		output_raw((uint8_t *)f->data, f->datalen);
	} else if(IS_U(f->lcf)) {
		fprintf(outf, "U: mfunc=%02x (%s) P/F=%x\n", U_MFUNC(f->lcf), U_cmd[U_MFUNC(f->lcf)], U_PF(f->lcf));
		output_avlc_U(f);
	} else {	// IS_I == true
		fprintf(outf, "I: sseq=0x%x rseq=0x%x poll=%x\n", f->lcf.I.send_seq, f->lcf.I.recv_seq, f->lcf.I.poll);
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
// vim: ts=4
