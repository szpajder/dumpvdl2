/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2023 Tomasz Lemiech <szpajder@gmail.com>
 *  Copyright (c) 2024 Thibaut VARENE <hacks@slashdirt.org>
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
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <libacars/libacars.h>      // la_msg_dir
#include <libacars/vstring.h>       // la_vstring
#include "dumpvdl2.h"
#include "config.h"

#define STATSD_UDP_BUFSIZE	1432	///< udp buffer size. Untold rule seems to be that the datagram must not be fragmented.

#define STATSD_NAMESPACE "dumpvdl2"

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
	"decoder.crc.bad",
	"decoder.errors.bitstream",
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
	"decoder.msg.good_loud",
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

static struct _statsd_runtime {
	char *namespace;		///< statsd namespace prefix (dot-terminated)
	struct sockaddr_storage ai_addr;
	socklen_t ai_addrlen;
	int sockfd;
} statsd_runtime = {};

typedef struct _statsd_runtime statsd_link;

static statsd_link *statsd = NULL;

static statsd_link *statsd_init_with_namespace(const char *host, const char *port, const char *ns)
{
	int sockfd;
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int ret;

	// obtain address(es) matching host/port
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;

	ret = getaddrinfo(host, port, &hints, &result);
	if (ret) {
		fprintf(stderr, "statsd: getaddrinfo: %s\n", gai_strerror(ret));
		return NULL;
	}

	// try each address until one succeeds
	for (rp = result; rp; rp = rp->ai_next) {
		sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (-1 != sockfd)
			break;	// success
	}

	if (!rp) {
		fprintf(stderr, "statsd: Could not reach server\n");
		goto cleanup;
	}

	memcpy(&statsd_runtime.ai_addr, rp->ai_addr, rp->ai_addrlen);	// ai_addrlen is guaranteed to be <= sizeof(sockaddr_storage)
	statsd_runtime.ai_addrlen = rp->ai_addrlen;

	ret = strlen(ns);
	statsd_runtime.namespace = malloc(ret + 2);
	if(!statsd_runtime.namespace) {
		perror("statsd");
		goto cleanup;
	}

	strcpy(statsd_runtime.namespace, ns);
	statsd_runtime.namespace[ret++] = '.';
	statsd_runtime.namespace[ret] = '\0';

	statsd_runtime.sockfd = sockfd;

	freeaddrinfo(result);
	return &statsd_runtime;

cleanup:
	freeaddrinfo(result);

	return NULL;
}

#ifdef DEBUG
static int statsd_validate(const char * stat)
{
	const char * p;
	for (p = stat; *p; p++) {
		switch (*p) {
			case ':':
			case '|':
			case '@':
				return (-1);
			default:
				;	// nothing
		}
	}

	return 0;
}
#endif

struct statsd_metric {
	enum { STATSD_UCOUNTER, STATSD_IGAUGE, STATSD_FGAUGE, STATSD_TIMING } type;
	const char *name;
	union { unsigned long u; long l; float f; } value;
};

/**
 * Update StatsD metrics.
 * @param metrics an array of metrics to push to StatsD
 * @param nmetrics the array size
 * @return exec status
 */
static int statsd_update(const struct statsd_metric * const metrics, const unsigned int nmetrics)
{
	char sbuffer[STATSD_UDP_BUFSIZE];
	const char *mtype;
	char * buffer;
	bool zerofirst;
	int ret;
	ssize_t sent;
	size_t avail;
	unsigned int i;

	buffer = sbuffer;
	avail = STATSD_UDP_BUFSIZE;

	for (i = 0; i < nmetrics; i++) {
#ifdef DEBUG
		if ((statsd_validate(metrics[i].name) != 0)) {
			fprintf(stderr, "statsd: ignoring invalid name \"%s\"", metrics[i].name);
			continue;
		}
#endif

		zerofirst = false;

		switch (metrics[i].type) {
			case STATSD_IGAUGE:
				mtype = "g";
				if (metrics[i].value.l < 0)
					zerofirst = true;
				break;
			case STATSD_FGAUGE:
				mtype = "g";
				if (metrics[i].value.f < 0.0F)
					zerofirst = true;
				break;
			case STATSD_UCOUNTER:
				mtype = "c";
				break;
			case STATSD_TIMING:
				mtype = "ms";
				break;
			default:
				ret = -1;
				goto cleanup;
		}

restartzero:
		// StatsD has a schizophrenic idea of what a gauge is (negative values are subtracted from previous data and not registered as is): work around its dementia
		if (zerofirst) {
			ret = snprintf(buffer, avail, "%s%s:0|%s\n", statsd_runtime.namespace ? statsd_runtime.namespace : "", metrics[i].name, mtype);
			if (ret < 0) {
				ret = -1;
				goto cleanup;
			}
			else if ((size_t)ret >= avail) {
				// send what we have, reset buffer, restart - no need to add '\0': sendto will truncate anyway
				sendto(statsd_runtime.sockfd, sbuffer, STATSD_UDP_BUFSIZE - avail, 0, (struct sockaddr *)&statsd_runtime.ai_addr, statsd_runtime.ai_addrlen);
				buffer = sbuffer;
				avail = STATSD_UDP_BUFSIZE;
				goto restartzero;
			}
			buffer += ret;
			avail -= (size_t)ret;
		}

restartbuffer:
		switch (metrics[i].type) {
			case STATSD_IGAUGE:
				ret = snprintf(buffer, avail, "%s%s:%ld|%s\n", statsd_runtime.namespace ? statsd_runtime.namespace : "", metrics[i].name, metrics[i].value.l, mtype);
				break;
			case STATSD_UCOUNTER:
			case STATSD_TIMING:
				ret = snprintf(buffer, avail, "%s%s:%lu|%s\n", statsd_runtime.namespace ? statsd_runtime.namespace : "", metrics[i].name, metrics[i].value.u, mtype);
				break;
			case STATSD_FGAUGE:
				ret = snprintf(buffer, avail, "%s%s:%f|%s\n", statsd_runtime.namespace ? statsd_runtime.namespace : "", metrics[i].name, metrics[i].value.f, mtype);
				break;
			default:
				ret = 0;
				break;	// cannot happen thanks to previous switch()
		}

		if (ret < 0) {
			ret = -1;
			goto cleanup;
		}
		else if ((size_t)ret >= avail) {
			// send what we have, reset buffer, restart - no need to add '\0': sendto will truncate anyway
			sendto(statsd_runtime.sockfd, sbuffer, STATSD_UDP_BUFSIZE - avail, 0, (struct sockaddr *)&statsd_runtime.ai_addr, statsd_runtime.ai_addrlen);
			buffer = sbuffer;
			avail = STATSD_UDP_BUFSIZE;
			goto restartbuffer;
		}
		buffer += ret;
		avail -= (size_t)ret;
	}

	ret = 0;

cleanup:
	// we only check for sendto() errors here
	sent = sendto(statsd_runtime.sockfd, sbuffer, STATSD_UDP_BUFSIZE - avail, 0, (struct sockaddr *)&statsd_runtime.ai_addr, statsd_runtime.ai_addrlen);
	if (-1 == sent)
		perror("statsd");

	return ret;
}

int statsd_count(__attribute__ ((unused)) statsd_link *link, char *stat, unsigned long value, __attribute__ ((unused)) float sample_rate)
{
	struct statsd_metric m = {
		.type = STATSD_UCOUNTER,
		.name = stat,
		.value.u = value,
	};

	return statsd_update(&m, 1);
}

int statsd_inc(statsd_link *link, char *stat, float sample_rate)
{
	return statsd_count(link, stat, 1, sample_rate);
}

int statsd_gauge(__attribute__ ((unused)) statsd_link *link, char *stat, long value)
{
	struct statsd_metric m = {
		.type = STATSD_IGAUGE,
		.name = stat,
		.value.l = value,
	};

	return statsd_update(&m, 1);
}

int statsd_timing(__attribute__ ((unused)) statsd_link *link, char *stat, unsigned long ms)
{
	struct statsd_metric m = {
		.type = STATSD_TIMING,
		.name = stat,
		.value.u = ms,
	};

	return statsd_update(&m, 1);

}

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
	la_vstring *statsd_namespace = la_vstring_new();
	la_vstring_append_sprintf(statsd_namespace, "%s", STATSD_NAMESPACE);
	if(Config.station_id != NULL) {
		fprintf(stderr, "Using extended statsd namespace %s.%s\n", STATSD_NAMESPACE, Config.station_id);
		la_vstring_append_sprintf(statsd_namespace, ".%s", Config.station_id);
	}
	statsd = statsd_init_with_namespace(addr, port, statsd_namespace->str);
	la_vstring_destroy(statsd_namespace, true);
	if(statsd == NULL) {
		return -2;
	}
	return 0;
}

void statsd_initialize_counters_per_channel(uint32_t freq) {
	if(statsd == NULL) {
		return;
	}
	char metric[256];
	for(int n = 0; counters_per_channel[n] != NULL; n++) {
		snprintf(metric, sizeof(metric), "%u.%s", freq, counters_per_channel[n]);
		statsd_count(statsd, metric, 0, 1.0);
	}
}

static void _statsd_initialize_counters_for_msg_dir(char const *counters[], la_msg_dir msg_dir) {
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

void statsd_counter_per_channel_increment(uint32_t freq, char *counter) {
	if(statsd == NULL) {
		return;
	}
	char metric[256];
	snprintf(metric, sizeof(metric), "%d.%s", freq, counter);
	statsd_inc(statsd, metric, 1.0);
}

void statsd_counter_per_msgdir_increment(la_msg_dir msg_dir, char *counter) {
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

void statsd_gauge_set(char *gauge, long value) {
	if(statsd == NULL) {
		return;
	}
	statsd_gauge(statsd, gauge, value);
}

void statsd_timing_delta_per_channel_send(uint32_t freq, char *timer, struct timeval ts) {
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
