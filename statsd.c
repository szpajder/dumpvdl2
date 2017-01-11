#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <statsd/statsd-client.h>
#include "rtlvdl2.h"

#define STATSD_NAMESPACE "rtlvdl2"
static statsd_link *statsd = NULL;
static uint32_t frequency = 0;

static const char *counters[] = {
	"avlc.errors.bad_fcs",
	"avlc.errors.no_flag_end",
	"avlc.errors.no_flag_start",
	"avlc.errors.too_short",
	"avlc.frames.good",
	"avlc.frames.processed",
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
	"decoder.errors.no_preamble",
	"decoder.errors.truncated_octets",
	"decoder.errors.unstuff",
	"decoder.msg.good",
	"decoder.preambles.good",
	"demod.sync.good",
	NULL
};

int statsd_initialize(char *statsd_addr) {
	char *addr;
	char *port;

	if(statsd_addr == NULL)
		return -1;
	if((addr = strtok(statsd_addr, ":")) == NULL)
		return -1;
	if((port = strtok(NULL, ":")) == NULL)
		return -1;
	if((statsd = statsd_init_with_namespace(addr, atoi(port), STATSD_NAMESPACE)) == NULL)
		return -2;
	return 0;
}

void statsd_initialize_counters(uint32_t freq) {
	if(!statsd) return;
	char metric[256];
	for(int n = 0; counters[n] != NULL; n++) {
		snprintf(metric, sizeof(metric), "%u.%s", freq, counters[n]);
		statsd_count(statsd, metric, 0, 1.0);
	}
	frequency = freq;
}

void statsd_counter_increment(char *counter) {
	if(!statsd) return;
	char metric[256];
	snprintf(metric, sizeof(metric), "%d.%s", frequency, counter);
	statsd_inc(statsd, metric, 1.0);
}

void statsd_timing_delta_send(char *timer, struct timeval *ts) {
	if(!statsd || !ts) return;
	char metric[256];
	struct timeval te;
	uint32_t tdiff;
	gettimeofday(&te, NULL);
	if(te.tv_sec < ts->tv_sec || (te.tv_sec == ts->tv_sec && te.tv_usec < ts->tv_usec)) {
		debug_print("timediff is negative: ts.tv_sec=%lu ts.tv_usec=%lu te.tv_sec=%lu te.tv_usec=%lu\n",
			ts->tv_sec, ts->tv_usec, te.tv_sec, te.tv_usec);
		return;
	}
	tdiff = ((te.tv_sec - ts->tv_sec) * 1000000UL + te.tv_usec - ts->tv_usec) / 1000;
	debug_print("tdiff: %u ms\n", tdiff);
	snprintf(metric, sizeof(metric), "%d.%s", frequency, timer);
	statsd_timing(statsd, metric, tdiff);
}
