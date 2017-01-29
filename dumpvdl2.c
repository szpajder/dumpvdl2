#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <errno.h>
#if WITH_RTLSDR
#include "rtl.h"
#endif
#include "dumpvdl2.h"

static float levels[256];
int do_exit = 0;

void sighandler(int sig) {
	fprintf(stderr, "Got signal %d, exiting\n", sig);
	do_exit = 1;
#if WITH_RTLSDR
	rtl_cancel();
#endif
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
	int i, min1 = 0, min2 = 0, min_dist, pos;
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
	v->dm_phi = 0.f;
}

void levels_init() {
	for (int i=0; i<256; i++) {
		levels[i] = i-127.5f;
	}
}

static uint32_t calc_centerfreq(uint32_t freq, uint32_t source_rate) {
	return freq;
}

void demod(vdl2_state_t *v) {
	static const uint8_t graycode[ARITY] = { 0, 1, 3, 2, 6, 7, 5, 4 };
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

			if(v->bs->end - v->bs->start >= v->requested_bits) {
				debug_print("bitstream len=%u requested_bits=%u, launching frame decoder\n", v->bs->end - v->bs->start, v->requested_bits);
				decode_vdl_frame(v);
				if(v->decoder_state == DEC_IDLE) {	// decoding finished or failed
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
	float cwf, swf;
	static float lp_re = 0.0f, lp_im = 0.0f;
	static const float iq_lp2 = 1.0f - IQ_LP;
	vdl2_state_t *v = (vdl2_state_t *)ctx;
	if(len == 0) return;
	samplenum = -1;
	for(i = 0; i < len;) {
#if DEBUG
		samplenum++;
#endif

		re = levels[buf[i]]; i++;
		im = levels[buf[i]]; i++;
// downmix
		if(v->offset_tuning) {
			sincosf_lut(v->dm_phi, &swf, &cwf);
			multiply(re, im, cwf, swf, &re, &im);
			v->dm_phi += v->dm_dphi;
			v->dm_phi &= 0xffffff;
		}

// lowpass IIR
		lp_re = IQ_LP * lp_re + iq_lp2 * re;
		lp_im = IQ_LP * lp_im + iq_lp2 * im;
// decimation
		cnt %= v->oversample;
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
			if(available < v->requested_samples)
				continue;

			debug_print("%d samples collected, doing demod\n", available);
			demod(v);
		}
	}
	bufnum++;
	if(DEBUG && bufnum % 10 == 0)
		debug_print("noise_floor: %f\n", v->mag_nf);
}

void process_file(void *ctx, char *path) {
	FILE *f;
	uint32_t len;
	unsigned char buf[FILE_BUFSIZE];
	
	if((f = fopen(path, "r")) == NULL) {
		perror("fopen()");
		_exit(2);
	}
	do {
		len = fread(buf, 1, FILE_BUFSIZE, f);
		process_samples(buf, len, ctx);
	} while(len == FILE_BUFSIZE && !do_exit);
	fclose(f);
}

vdl2_state_t *vdl2_init(uint32_t centerfreq, uint32_t freq, uint32_t source_rate, uint32_t oversample) {
	vdl2_state_t *v;
	v = XCALLOC(1, sizeof(vdl2_state_t));
	v->bs = bitstream_init(BSLEN);
	v->mag_nf = 100.0f;
// Cast to signed first, because casting negative float to uint is not portable
	v->dm_dphi = (uint32_t)(int)(((float)centerfreq - (float)freq) / (float)source_rate * 256.0f * 65536.0f);
	debug_print("dm_dphi: 0x%x\n", v->dm_dphi);
	v->offset_tuning = (centerfreq != freq);
	v->oversample = oversample;
	demod_reset(v);
	return v;
}

void usage() {
	fprintf(stderr, "DUMPVDL2 version %s\n", DUMPVDL2_VERSION);
	fprintf(stderr, "Usage:\n\n");
#if WITH_RTLSDR
	fprintf(stderr, "RTLSDR receiver:\n");
	fprintf(stderr, "\tdumpvdl2 [output_options] -R <device_id> [rtlsdr_options] [<channel_frequency>]\n");
#endif
	fprintf(stderr, "I/Q input from file:\n");
	fprintf(stderr, "\tdumpvdl2 [output_options] -F <input_file> [file_options] [<channel_frequency>]\n");
	fprintf(stderr, "\ncommon options:\n");
	fprintf(stderr, "\t<channel_frequency>\tVDL2 channel to listen to, In Hz.\n");
	fprintf(stderr, "\t\t\t\tIf omitted, will use VDL2 Common Signalling Channel (%u Hz)\n", CSC_FREQ);
	fprintf(stderr, "\noutput_options:\n");
	fprintf(stderr, "\t-o <output_file>\tOutput decoded frames to <output_file> (default: stdout)\n");
	fprintf(stderr, "\t-H\t\t\tRotate output file hourly\n");
	fprintf(stderr, "\t-D\t\t\tRotate output file daily\n");
#if USE_STATSD
	fprintf(stderr, "\t-S <host>:<port>\tSend statistics to Etsy StatsD server <host>:<port> (default: disabled)\n");
#endif
#if WITH_RTLSDR
	fprintf(stderr, "\nrtlsdr_options:\n");
	fprintf(stderr, "\t-R <device_id>\t\tUse RTL device with specified ID (default: 0)\n");
	fprintf(stderr, "\t-g <gain>\t\tSet RTL gain (decibels)\n");
	fprintf(stderr, "\t-p <correction>\t\tSet RTL freq correction (ppm)\n");
	fprintf(stderr, "\t-c <center_frequency>\tSet RTL center frequency in Hz (default: auto)\n");
#endif
	fprintf(stderr, "\nfile_options:\n");
	fprintf(stderr, "\t-F <input_file>\t\tRead I/Q samples from file (must be sampled at %u samples/sec)\n", SYMBOL_RATE * SPS * FILE_OVERSAMPLE);
	fprintf(stderr, "\t-c <center_frequency>\tCenter frequency of the input data, in Hz (default: 0)\n");
	_exit(0);
}

int main(int argc, char **argv) {
	vdl2_state_t *ctx;
	uint32_t freq = 0, centerfreq = 0, sample_rate = 0, oversample = 0;
	enum input_types input = INPUT_UNDEF;
#if WITH_RTLSDR
	uint32_t device = 0;
// FIXME: default gain and correction depend on receiver type which is currently enabled
	int gain = RTL_AUTO_GAIN;
	int correction = 0;
#endif
	int opt;
	char *optstring = "c:DF:g:hHo:p:R:S:";
#if USE_STATSD
	char *statsd_addr = NULL;
	int statsd_enabled = 0;
#endif
	char *infile = NULL, *outfile = NULL;

	while((opt = getopt(argc, argv, optstring)) != -1) {
		switch(opt) {
		case 'F':
			infile = strdup(optarg);
			input = INPUT_FILE;
			oversample = FILE_OVERSAMPLE;
			break;
		case 'H':
			hourly = 1;
			break;
		case 'D':
			daily = 1;
			break;
		case 'c':
			centerfreq = strtoul(optarg, NULL, 10);
			break;
#if WITH_RTLSDR
		case 'R':
			device = strtoul(optarg, NULL, 10);
			input = INPUT_RTLSDR;
			oversample = RTL_OVERSAMPLE;
			break;
		case 'g':
			gain = (int)(10 * atof(optarg));
			break;
		case 'p':
			correction = atoi(optarg);
			break;
#endif
		case 'o':
			outfile = strdup(optarg);
			break;
#if USE_STATSD
		case 'S':
			statsd_addr = strdup(optarg);
			statsd_enabled = 1;
			break;
#endif
		case 'h':
		default:
			usage();
		}
	}
	if(optind < argc)
		freq = strtoul(argv[optind], NULL, 10);

	if(input == INPUT_UNDEF)
		usage();
	if(outfile == NULL) {
		outfile = strdup("-");		// output to stdout by default
		hourly = daily = 0;		// stdout is not rotateable - ignore silently
	}
	if(outfile != NULL && hourly && daily) {
		fprintf(stderr, "Options: -H and -D are exclusive\n");
		fprintf(stderr, "Use -h for help\n");
		_exit(1);
	}
	if(freq == 0) {
		fprintf(stderr, "Warning: frequency not set - using VDL2 Common Signalling Channel as a default (%u Hz)\n", CSC_FREQ);
		freq = CSC_FREQ;
	}
	sample_rate = SYMBOL_RATE * SPS * oversample;
	fprintf(stderr, "Sampling rate set to %u symbols/sec\n", sample_rate);
	if(centerfreq == 0) {
		centerfreq = calc_centerfreq(freq, sample_rate);
		if(centerfreq == 0) {
			fprintf(stderr, "Failed to calculate center frequency\n");
			_exit(2);
		}
	}
	if((ctx = vdl2_init(centerfreq, freq, sample_rate, oversample)) == NULL) {
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
	if(init_output_file(outfile) < 0) {
		fprintf(stderr, "Failed to initialize output - aborting\n");
		_exit(4);
	}
	setup_signals();
	levels_init();
	sincosf_lut_init();
	switch(input) {
	case INPUT_FILE:
		process_file(ctx, infile);
		break;
#if WITH_RTLSDR
	case INPUT_RTLSDR:
		rtl_init(ctx, device, centerfreq, gain, correction);
		break;
#endif
	default:
		fprintf(stderr, "Unknown input type\n");
		_exit(5);
	}
	return(0);
}
