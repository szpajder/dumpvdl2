#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include "rtlvdl2.h"

const uint8_t graycode[ARITY] = { 0, 1, 3, 2, 6, 7, 5, 4 };

/* crude correlator */
int correlate_and_sync(float *buf, uint32_t len) {
	int i, min1, min2, min_dist, pos;
	float avgmax, minv1, minv2;
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
		return -1;
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
		return -1;
	}
/* Get notch distance (shall equal 4 symbol periods) */
/* Allow some clock variance */
	min_dist = min2 - min1;
	if((float)min_dist > 1.1f * 4.0f * (float)SPS) {
		debug_print("min_dist %d too high\n", min_dist);
		return -1;
	}
	if((float)min_dist < 0.9f * 4.0f * (float)SPS) {
		debug_print("min_dist %d too low\n", min_dist);
		return -1;
	}
/* Steady transmitter state starts 5.5 symbol periods before first notch. */
/* Skip one symbol if pos is slightly negative (ie. squelch opened a bit too late) */
	pos = min1 - (int)(round(5.5f * (float)SPS));
	if(pos < 0) pos += SPS;
	if(pos < 0) {
		debug_print("pos is negative: %d\n", pos-SPS);
		return -1;
	}
	debug_print("avgmax: %f, min1: %f @ %d, min2: %f @ %d, min_dist: %d pos; %d\n", avgmax, minv1, min1, minv2, min2, min_dist, pos);
	return pos;
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
/*	if(v->symcnt > 0) printf("[%d symbols]\n\n", v->symcnt);
	v->symcnt = 0; */
}

void demod(vdl2_state_t *v) {
	float dI, dQ, dphi;
	int idx, samples_available, samples_needed;

	if(v->decoder_state == DEC_IDLE) {
		debug_print("%s", "demod: decoder_state is DEC_IDLE, switching to DM_IDLE\n");
		v->demod_state = DM_IDLE;
		return;
	}

	switch(v->demod_state) {
	case DM_INIT:
		v->sclk = v->bufs = correlate_and_sync(v->mag_buf, v->bufe);
		if(v->sclk < 0) {		/* no sync */
			v->demod_state = DM_IDLE;
			debug_print("%s", "no sync, DM_IDLE\n");
			return;
		}
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
			if(dphi < 0) dphi += 2.0 * M_PI;
			dphi /= M_PI_4;
			idx = (int)roundf(dphi) % ARITY;
//			debug_print("sclk: %d bufs: %d bufe: %d dphi: %f * pi/4 idx: %d bits: %d\n", v->sclk, v->bufs, v->bufe, dphi, idx, graycode[idx]);
//			printf("%d ", graycode[idx]);
//			v->symcnt++;
// FIXME: msb czy lsb?
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
				debug_print("avail: %d bufs: %d bufe: %d sclk: %d\n", samples_available, v->bufs, v->bufe, v->sclk);
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
	static int bufnum = 0, samplenum = 0;
	float re, im, mag;
	vdl2_state_t *v = (vdl2_state_t *)ctx;
	if(len == 0) return;
	samplenum = -1;
	for(i = 0; i < len;) {
		samplenum++;
		re = (float)buf[i] - 127.5f; i++;
		im = (float)buf[i] - 127.5f; i++;
		mag = hypotf(re, im);
		v->mag_lp = v->mag_lp * MAG_LPSLOW + mag * (1.0f - MAG_LPSLOW);
		if(v->mag_lp > 3.0f) {
			if(v->demod_state == DM_IDLE) {
				idle_skips++;
				continue;
			}
			if(v->sq == 0) {
				debug_print("*** on at (%d:%d) ***\n", bufnum, samplenum);
				v->sq = 1;
				idle_skips = not_idle_skips = 0;
			}
		} else if(v->mag_lp < 3.0f) {
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
			v->I[v->bufe] = re;
			v->Q[v->bufe] = im;
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
}

void process_file(char *path, void *ctx) {
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
	} while(len == RTL_BUFSIZE);
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
	demod_reset(v);
	return v;
}

int main(int argc, char **argv) {
	vdl2_state_t *ctx;
	if(argc < 2) {
		fprintf(stderr, "Usage: rtlvdl2 <file>\n");
		_exit(1);
	}
	if((ctx = vdl2_init()) == NULL) {
		fprintf(stderr, "Failed to initialize VDL state\n");
		_exit(2);
	}
	if(rs_init() < 0) {
		fprintf(stderr, "Failed to initialize RS codec\n");
		_exit(3);
	}
	process_file(argv[1], ctx);
	return(0);
}

// vim: ts=4
