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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <statsd/statsd-client.h>
#include <libacars/libacars.h>      // la_msg_dir
#include "dumpvdl2.h"

#define STATSD_NAMESPACE "dumpvdl2"
static statsd_link *statsd = NULL;

static char const *counters_per_channel[] = {
	"avlc.errors.bad_fcs",
	"avlc.errors.too_short",
	"avlc.frames.good",
	"avlc.frames.processed",
	"avlc.msg.air2air",
	"avlc.msg.air2all",
	"avlc.msg.air2gnd",
	"avlc.msg.gnd2air",
	"avlc.msg.gnd2all",
	"avlc.msg.gnd2gnd",
	"decoder.blocks.fec_ok",
	"decoder.blocks.processed",
	"decoder.crc.good",
	"decoder.errors.bitstream",
	"decoder.errors.bitstream",
	"decoder.errors.crc_bad",
	"decoder.errors.data_truncated",
	"decoder.errors.deinterleave_data",
	"decoder.errors.deinterleave_fec",
	"decoder.errors.fec_bad",
	"decoder.errors.fec_truncated",
	"decoder.errors.no_fec",
	"decoder.errors.no_header",
	"decoder.errors.too_long",
	"decoder.errors.truncated_octets",
	"decoder.errors.unstuff",
	"decoder.msg.good",
	"decoder.preambles.good",
	"demod.sync.good",
	NULL
};

static char const *counters_per_msgdir[] = {
	"acars.reasm.unknown",
	"acars.reasm.complete",
	// "acars.reasm.in_progress",   // we report final reasm states only
	"acars.reasm.skipped",
	"acars.reasm.duplicate",
	"acars.reasm.out_of_seq",
	"acars.reasm.invalid_args",
	"x25.reasm.unknown",
	"x25.reasm.complete",
	// "x25.reasm.in_progress",     // we report final reasm states only
	"x25.reasm.skipped",
	"x25.reasm.duplicate",
	"x25.reasm.out_of_seq",
	"x25.reasm.invalid_args",
	NULL
};

static char const *msg_dir_labels[] = {
	[LA_MSG_DIR_UNKNOWN] = "unknown",
	[LA_MSG_DIR_AIR2GND] = "air2gnd",
	[LA_MSG_DIR_GND2AIR] = "gnd2air"
};

int statsd_initialize(char *statsd_addr) {
	char *addr;
	char *port;

	if(statsd_addr == NULL) {
		return -1;
	}
	if((addr = strtok(statsd_addr, ":")) == NULL) {
		return -1;
	}
	if((port = strtok(NULL, ":")) == NULL) {
		return -1;
	}
	if((statsd = statsd_init_with_namespace(addr, atoi(port), STATSD_NAMESPACE)) == NULL) {
		return -2;
	}
	return 0;
}

void statsd_initialize_counters_per_channel(uint32_t const freq) {
	if(statsd == NULL) {
		return;
	}
	char metric[256];
	for(int n = 0; counters_per_channel[n] != NULL; n++) {
		snprintf(metric, sizeof(metric), "%u.%s", freq, counters_per_channel[n]);
		statsd_count(statsd, metric, 0, 1.0);
	}
}

static void _statsd_initialize_counters_for_msg_dir(char const *counters[], la_msg_dir const msg_dir) {
	char metric[256];
	for(int n = 0; counters[n] != NULL; n++) {
		snprintf(metric, sizeof(metric), "%s.%s", counters[n], msg_dir_labels[msg_dir]);
		statsd_count(statsd, metric, 0, 1.0);
	}
}

void statsd_initialize_counters_per_msgdir() {
	if(statsd == NULL) {
		return;
	}
	_statsd_initialize_counters_for_msg_dir(counters_per_msgdir, LA_MSG_DIR_AIR2GND);
	_statsd_initialize_counters_for_msg_dir(counters_per_msgdir, LA_MSG_DIR_GND2AIR);
}

void statsd_initialize_counter_set(char **counter_set) {
	if(statsd == NULL) {
		return;
	}
	for(int n = 0; counter_set[n] != NULL; n++) {
		statsd_count(statsd, counter_set[n], 0, 1.0);
	}
}

void statsd_counter_per_channel_increment(uint32_t const freq, char *counter) {
	if(statsd == NULL) {
		return;
	}
	char metric[256];
	snprintf(metric, sizeof(metric), "%d.%s", freq, counter);
	statsd_inc(statsd, metric, 1.0);
}

void statsd_counter_per_msgdir_increment(la_msg_dir const msg_dir, char *counter) {
	if(statsd == NULL) {
		return;
	}
	char metric[256];
	snprintf(metric, sizeof(metric), "%s.%s", counter, msg_dir_labels[msg_dir]);
	statsd_inc(statsd, metric, 1.0);
}

void statsd_counter_increment(char *counter) {
	if(statsd == NULL) {
		return;
	}
	statsd_inc(statsd, counter, 1.0);
}

void statsd_gauge_set(char *gauge, size_t value) {
	if(statsd == NULL) {
		return;
	}
	statsd_gauge(statsd, gauge, value);
}

void statsd_timing_delta_per_channel_send(uint32_t const freq, char *timer, struct timeval const ts) {
	if(statsd == NULL) {
		return;
	}
	char metric[256];
	struct timeval te;
	uint32_t tdiff;
	gettimeofday(&te, NULL);
	if(te.tv_sec < ts.tv_sec || (te.tv_sec == ts.tv_sec && te.tv_usec < ts.tv_usec)) {
		debug_print(D_STATS, "timediff is negative: ts.tv_sec=%lu ts.tv_usec=%lu te.tv_sec=%lu te.tv_usec=%lu\n",
				ts.tv_sec, ts.tv_usec, te.tv_sec, te.tv_usec);
		return;
	}
	tdiff = ((te.tv_sec - ts.tv_sec) * 1000000UL + te.tv_usec - ts.tv_usec) / 1000;
	debug_print(D_STATS, "tdiff: %u ms\n", tdiff);
	snprintf(metric, sizeof(metric), "%d.%s", freq, timer);
	statsd_timing(statsd, metric, tdiff);
}
