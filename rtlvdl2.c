#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <errno.h>
#include <rtl-sdr.h>
#include "rtlvdl2.h"

const uint8_t graycode[ARITY] = { 0, 1, 3, 2, 6, 7, 5, 4 };
static rtlsdr_dev_t *rtl = NULL;
int do_exit = 0;

void sighandler(int sig) {
	fprintf(stderr, "Got signal %d, exiting\n", sig);
	do_exit = 1;
	if(rtl != NULL)
		rtlsdr_cancel_async(rtl);
}

void setup_signals() {
	struct sigaction sigact, pipeact;

	memset(&sigact, 0, sizeof(sigact));
	memset(&pipeact, 0, sizeof(pipeact));
	pipeact.sa_handler = SIG_IGN;
	sigact.sa_handler = &sighandler;
	sigaction(SIGPIPE, &pipeact, NULL);
	sigaction(SIGHUP, &sigact, NULL);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
}

/* crude correlator */
void correlate_and_sync(vdl2_state_t *v) {
	int i, min1, min2, min_dist, pos;
	float avgmax, minv1, minv2;
	float *buf = v->mag_buf;
	v->sclk = -1;
/* Average power over first 3 symbol periods */
	for(avgmax = 0, i = 0; i < 3 * SPS; i++) {
		avgmax += buf[i];
	}
	avgmax /= 3 * SPS;
/* Search for a first notch over first 7 symbol periods
 * (it's actually a second notch in the preamble, because it's always
 * deeper than the first one). Reject it if it's not deep enough. */
	minv1 = avgmax;
	for(i = 0; i < 7 * SPS; i++) {
		if(buf[i] < minv1) {
			minv1 = buf[i];
			min1 = i;
		}
	}
	if(3 * minv1 >= avgmax) {
		debug_print("min1=%f at pos %d too high (avgmax=%f)\n", minv1, min1, avgmax);
		return;
	}
/* Search for a notch over 8-11 symbol periods */
	minv2 = avgmax;
	for(i = 7 * SPS; i < SYNC_SYMS * SPS; i++) {
		if(buf[i] < minv2) {
			minv2 = buf[i];
			min2 = i;
		}
	}
	if(3 * minv2 >= avgmax) {
		debug_print("min2=%f at pos %d too high (avgmax=%f)\n", minv2, min2, avgmax);
		return;
	}
/* Get notch distance (shall equal 4 symbol periods) */
/* Allow some clock variance */
	min_dist = min2 - min1;
	if((float)min_dist > 1.1f * 4.0f * (float)SPS) {
		debug_print("min_dist %d too high\n", min_dist);
		return;
	}
	if((float)min_dist < 0.9f * 4.0f * (float)SPS) {
		debug_print("min_dist %d too low\n", min_dist);
		return;
	}
/* Steady transmitter state starts 5.5 symbol periods before first notch. */
/* Skip one symbol if pos is slightly negative (ie. squelch opened a bit too late) */
	pos = min1 - (int)(round(5.5f * (float)SPS));
	if(pos < 0) pos += SPS;
	if(pos < 0) {
		debug_print("pos is negative: %d\n", pos-SPS);
		return;
	}
	debug_print("avgmax: %f, min1: %f @ %d, min2: %f @ %d, min_dist: %d pos: %d mag_nf: %f\n",
		avgmax, minv1, min1, minv2, min2, min_dist, pos, v->mag_nf);
	v->mag_frame = avgmax;
	v->sclk = v->bufs = pos;
}

void multiply(float ar, float aj, float br, float bj, float *cr, float *cj) {
	*cr = ar*br - aj*bj;
	*cj = aj*br + ar*bj;
}

void decoder_reset(vdl2_state_t *v) {
	v->decoder_state = DEC_PREAMBLE;
	bitstream_reset(v->bs);
	v->requested_bits = 4 * BPS + PREAMBLE_LEN;		// allow some extra room for leading zeros in xmtr ramp-up stage
}

void demod_reset(vdl2_state_t *v) {
	decoder_reset(v);
	v->bufe = v->bufs = v->sclk = 0;
	v->demod_state = DM_INIT;
	v->requested_samples = SYNC_SYMS * SPS;
}

void demod(vdl2_state_t *v) {
	float dI, dQ, dphi, phierr;
	int idx, samples_available, samples_needed;

	if(v->decoder_state == DEC_IDLE) {
		debug_print("%s", "demod: decoder_state is DEC_IDLE, switching to DM_IDLE\n");
		v->demod_state = DM_IDLE;
		return;
	}

	switch(v->demod_state) {
	case DM_INIT:
		correlate_and_sync(v);
		if(v->sclk < 0) {		/* no sync */
			v->demod_state = DM_IDLE;
			debug_print("%s", "no sync, DM_IDLE\n");
			return;
		}
		statsd_increment("demod.sync.good");
		v->dphi = 0.0f;
		v->pI = v->I[v->sclk];
		v->pQ = v->Q[v->sclk];
		v->demod_state = DM_SYNC;
		v->requested_samples = PREAMBLE_SYMS * SPS;
		debug_print("%s", "DM_SYNC\n");
		return;
	case DM_SYNC:
		v->bufs = v->sclk;
		samples_available = v->bufe - v->bufs;
		if(samples_available < 0) samples_available += BUFSIZE;
		for(;;) {
			multiply(v->I[v->sclk], v->Q[v->sclk], v->pI, -(v->pQ), &dI, &dQ);
			dphi = atan2(dQ, dI);
			dphi -= v->dphi;
			if(dphi < 0) dphi += 2.0f * M_PI;
			dphi /= M_PI_4;
			phierr = (dphi - roundf(dphi)) * M_PI_4;
			v->dphi = DPHI_LP * v->dphi + (1.0f - DPHI_LP) * phierr;
			idx = (int)roundf(dphi) % ARITY;
			debug_print("sclk: %d I: %f Q: %f dphi: %f * pi/4 idx: %d bits: %d phierr: %f v->dphi: %f\n",
				v->sclk, v->I[v->sclk], v->Q[v->sclk], dphi, idx, graycode[idx], phierr, v->dphi);
			if(bitstream_append_msbfirst(v->bs, &(graycode[idx]), 1, BPS) < 0) {
				debug_print("%s", "bitstream_append_msbfirst failed\n");
				v->demod_state = DM_IDLE;
				return;
			}
			v->pI = v->I[v->sclk];
			v->pQ = v->Q[v->sclk];

			v->sclk += SPS; v->sclk %= BUFSIZE;
			samples_available -= SPS;

//			debug_print("bs len: %u req bits: %u\n", v->bs->end - v->bs->start, v->requested_bits);
			if(v->bs->end - v->bs->start >= v->requested_bits) {
				debug_print("bitstream len=%u requested_bits=%u, launching frame decoder\n", v->bs->end - v->bs->start, v->requested_bits);
				decode_vdl_frame(v);
				if(v->decoder_state == DEC_IDLE) {		// decoding finished or failed
					v->demod_state = DM_IDLE;
					return;
				} else {
					samples_needed = (v->requested_bits / BPS + 1) * SPS;
					if(samples_available < samples_needed) {
						debug_print("decoder needs %d bits (%d samples), having only %d samples - requesting additional %d samples\n",
							v->requested_bits, samples_needed, samples_available, samples_needed - samples_available);
						v->requested_samples = samples_needed - samples_available;
						if(v->requested_samples > BUFSIZE)
							v->requested_samples = BUFSIZE - 1;
					}
				}
			}

			if(samples_available <= 0) {
//				debug_print("avail: %d bufs: %d bufe: %d sclk: %d\n", samples_available, v->bufs, v->bufe, v->sclk);
				v->bufs = v->bufe;
				break;
			}
			v->bufs = v->sclk;
		}
		return;
	case DM_IDLE:
		return;
	}
}

void process_samples(unsigned char *buf, uint32_t len, void *ctx) {
	int i, available;
	static int idle_skips = 0, not_idle_skips = 0;
	static int bufnum = 0, samplenum = 0, cnt = 0, nfcnt = 0;
	float re, im, mag;
	static float lp_re = 0.0f, lp_im = 0.0f;
	static const float iq_lp2 = 1.0f - IQ_LP;
	vdl2_state_t *v = (vdl2_state_t *)ctx;
	if(len == 0) return;
	samplenum = -1;
	for(i = 0; i < len;) {
		samplenum++;

		re = (float)buf[i] - 127.5f; i++;
		im = (float)buf[i] - 127.5f; i++;
// lowpass IIR
		lp_re = IQ_LP * lp_re + iq_lp2 * re;
		lp_im = IQ_LP * lp_im + iq_lp2 * im;
// decimation
		cnt %= RTL_OVERSAMPLE;
		if(cnt++ != 0)
			continue;

		mag = hypotf(lp_re, lp_im);
		v->mag_lp = v->mag_lp * MAG_LP + mag * (1.0f - MAG_LP);
		nfcnt %= 1000;
// update noise floor estimate
		if(nfcnt++ == 0)
			v->mag_nf = NF_LP * v->mag_nf + (1.0f - NF_LP) * fminf(v->mag_lp, v->mag_nf) + 0.0001f;
		if(v->mag_lp > 3.0f * v->mag_nf) {
			if(v->demod_state == DM_IDLE) {
				idle_skips++;
				continue;
			}
			if(v->sq == 0) {
				debug_print("*** on at (%d:%d) ***\n", bufnum, samplenum);
				v->sq = 1;
				idle_skips = not_idle_skips = 0;
			}
		} else {
			if(v->sq == 1 && v->demod_state == DM_IDLE) {	// close squelch only when decoder finished work or errored
															// FIXME: time-limit this, because reading obvious trash does not make sense
				debug_print("*** off at (%d:%d) *** after %d idle_skips, %d not_idle_skips\n", bufnum, samplenum, idle_skips, not_idle_skips);
				v->sq = 0;
				demod_reset(v);
			} else {
				not_idle_skips++;
			}
		}
		if(v->sq == 1) {
			v->I[v->bufe] = lp_re;
			v->Q[v->bufe] = lp_im;
			v->mag_buf[v->bufe] = mag;
			v->mag_lpbuf[v->bufe] = v->mag_lp;
			v->bufe++; v->bufe %= BUFSIZE;
//			debug_print("plot: %f %f\n", mag, v->mag_lp);

			available = v->bufe - v->bufs;
			if(available < 0 ) available += BUFSIZE;
			if(available < v->requested_samples) {
//				debug_print("not enough available samples: %d (requested samples: %d)\n", available, v->requested_samples);
				continue;
			}

			debug_print("%d samples collected, doing demod\n", available);
			demod(v);
		}
	}
	bufnum++;
	if(DEBUG && bufnum % 10 == 0)
		debug_print("noise_floor: %f\n", v->mag_nf);
}

void init_rtl(void *ctx, uint32_t device, int freq, int gain, int correction) {
	int r;

	r = rtlsdr_open(&rtl, device);
	if(rtl == NULL) {
		fprintf(stderr, "Failed to open rtlsdr device #%u: error %d\n", device, r);
		exit(1);
	}
	r = rtlsdr_set_sample_rate(rtl, RTL_RATE);
	if (r < 0) {
		fprintf(stderr, "Failed to set sample rate for device #%d: error %d\n", device, r);
		exit(1);
	}
	r = rtlsdr_set_center_freq(rtl, freq);
	if (r < 0) {
		fprintf(stderr, "Failed to set frequency for device #%d: error %d\n", device, r);
		exit(1);
	}
	r = rtlsdr_set_freq_correction(rtl, correction);
	if (r < 0 && r != -2 ) {
		fprintf(stderr, "Failed to set freq correction for device #%d error %d\n", device, r);
		exit(1);
	}

	if(gain == RTL_AUTO_GAIN) {
		r = rtlsdr_set_tuner_gain_mode(rtl, 0);
		if (r < 0) {
			fprintf(stderr, "Failed to set automatic gain for device #%d: error %d\n", device, r);
			exit(1);
		} else
			fprintf(stderr, "Device #%d: gain set to automatic\n", device);
	} else {
		r = rtlsdr_set_tuner_gain_mode(rtl, 1);
		r |= rtlsdr_set_tuner_gain(rtl, gain);
		if (r < 0) {
			fprintf(stderr, "Failed to set gain to %0.2f for device #%d: error %d\n",
				(float)rtlsdr_get_tuner_gain(rtl) / 10.0, device, r);
			exit(1);
		} else
			fprintf(stderr, "Device #%d: gain set to %0.2f dB\n", device,
				(float)rtlsdr_get_tuner_gain(rtl) / 10.0);
	}

	r = rtlsdr_set_agc_mode(rtl, 0);
	if (r < 0) {
		fprintf(stderr, "Failed to disable AGC for device #%d: error %d\n", device, r);
		exit(1);
	}
	rtlsdr_reset_buffer(rtl);
	fprintf(stderr, "Device %d started\n", device);
	if(rtlsdr_read_async(rtl, process_samples, ctx, RTL_BUFCNT, RTL_BUFSIZE) < 0) {
		fprintf(stderr, "Device #%d: async read failed\n", device);
		exit(1);
	}
}

void process_file(void *ctx, char *path) {
	FILE *f;
	uint32_t len;
	unsigned char buf[RTL_BUFSIZE];
	
	if((f = fopen(path, "r")) == NULL) {
		perror("fopen()");
		_exit(2);
	}
	do {
		len = fread(buf, 1, RTL_BUFSIZE, f);
		process_samples(buf, len, ctx);
	} while(len == RTL_BUFSIZE && !do_exit);
	fclose(f);
}

vdl2_state_t *vdl2_init() {
	vdl2_state_t *v;
	v = calloc(1, sizeof(vdl2_state_t));
	if(v == NULL) return NULL;
	v->bs = bitstream_init(BSLEN);
	if(v->bs == NULL) {
		debug_print("%s", "bitstream_init failed\n");
		free(v);
		return NULL;
	}
	v->mag_nf = 100.0f;
	demod_reset(v);
	return v;
}

void usage() {
	fprintf(stderr, "RTLVDL2 version %s\n", RTLVDL2_VERSION);
	fprintf(stderr, "Usage: rtlvdl2 [common_options] [rtlsdr_options] frequency_hz\n");
	fprintf(stderr, "       rtlvdl2 [common_options] -f <input_file>\n");
	fprintf(stderr, "\ncommon_options:\n");
	fprintf(stderr, "\t-o <output_file>\tOutput decoded frames to <output_file> (default: stdout)\n");
#if USE_STATSD
	fprintf(stderr, "\t-S <host>:<port>\tSend statistics to Etsy StatsD server <host>:<port> (default: disabled)\n");
#endif
	fprintf(stderr, "\nrtlsdr_options:\n");
	fprintf(stderr, "\t-d <device_id>\t\tUse specified device (default: 0)\n");
	fprintf(stderr, "\t-g <gain>\t\tSet RTL gain (decibels)\n");
	fprintf(stderr, "\t-p <correction>\t\tSet RTL freq correction (ppm)\n");
	_exit(1);
}

int main(int argc, char **argv) {
	vdl2_state_t *ctx;
	uint32_t device = 0;
	uint32_t freq = 0;
	int gain = RTL_AUTO_GAIN;
	int correction = 0;
	int opt;
	char *optstring = 
#if USE_STATSD
	"d:f:g:o:p:S:";
	char *statsd_addr;
	int statsd_enabled = 0;
#else
	"d:f:g:o:p:";
#endif
	char *infile = NULL, *outfile = NULL;

	while((opt = getopt(argc, argv, optstring)) != -1) {
		switch(opt) {
		case 'f':
			infile = strdup(optarg);
			break;
		case 'd':
			device = strtoul(optarg, NULL, 10);
			break;
		case 'g':
			gain = (int)(10 * atof(optarg));
			break;
		case 'o':
			outfile = strdup(optarg);
			break;
		case 'p':
			correction = atoi(optarg);
			break;
#if USE_STATSD
		case 'S':
			statsd_addr = strdup(optarg);
			statsd_enabled = 1;
			break;
#endif
		default:
			usage();
		}
	}
	if(optind < argc)
		freq = strtoul(argv[optind], NULL, 10);

	if(freq != 0 && infile != NULL) {
		fprintf(stderr, "Error: frequency and -f <file> options are exclusive\n");
		usage();
	} else if(freq == 0 && infile == NULL) {
		fprintf(stderr, "Error: either frequency or -f <file> option is required\n");
		usage();
	}
	if(outfile == NULL)
		outfile = strdup("-");		// output to stdout by default
	if(init_output_file(outfile) < 0) {
		fprintf(stderr, "Failed to initialize output - aborting\n");
		_exit(4);
	}
	if((ctx = vdl2_init()) == NULL) {
		fprintf(stderr, "Failed to initialize VDL state\n");
		_exit(2);
	}
	if(rs_init() < 0) {
		fprintf(stderr, "Failed to initialize RS codec\n");
		_exit(3);
	}
#if USE_STATSD
    if(statsd_enabled && freq != 0) {
		if(statsd_initialize(statsd_addr) < 0) {
				fprintf(stderr, "Failed to initialize statsd client\n");
				_exit(4);
		}
		statsd_initialize_counters(freq);
	} else {
		statsd_enabled = 0;
	}
#endif
	setup_signals();
	if(infile != NULL) {
		process_file(ctx, infile);
	} else {
		init_rtl(ctx, device, freq, gain, correction);
	}
	return(0);
}

// vim: ts=4
