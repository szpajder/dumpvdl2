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
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>		// calloc
#include <math.h>		// sincosf, hypotf, atan2
#include "chebyshev.h"		// chebyshev_lpf_init
#include "dumpvdl2.h"

static float *levels;
static float sin_lut[257], cos_lut[257];

// input lowpass filter design constants
#define INP_LPF_CUTOFF_FREQ 10000
#define INP_LPF_RIPPLE_PERCENT 0.5f
// do not change this; filtering routine is currently hardcoded to 2 poles to minimize CPU usage
#define INP_LPF_NPOLES 2
// filter coefficients
static float *A = NULL, *B = NULL;

// phi range must be (0..1), rescaled to 0x0-0xFFFFFF
static void sincosf_lut(uint32_t phi, float *sine, float *cosine) {
	float v1, v2, fract;
	uint32_t idx;
// get LUT index
	idx = phi >> 16;
// cast fixed point fraction to float
	fract = (float)(phi & 0xffff) / 65536.0f;
// get two adjacent values from LUT and interpolate
	v1 = sin_lut[idx];
	v2 = sin_lut[idx+1];
	*sine = v1 + (v2 - v1) * fract;
	v1 = cos_lut[idx];
	v2 = cos_lut[idx+1];
	*cosine = v1 + (v2 - v1) * fract;
}

static float chebyshev_lpf_2pole(float const * const in, float const * const out) {
	float r = A[0] * in[0];
	r += A[1] * in[1] + A[2] * in[2];
	r += B[1] * out[1] + B[2] * out[2];
	return r;
}

static void correlate_and_sync(vdl2_channel_t *v) {
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
	for(i = 2 * SPS; i < 7 * SPS; i++) {
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
		debug_print("min_dist %d too high (min1=%d min2=%d)\n", min_dist, min1, min2);
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

static void multiply(float ar, float aj, float br, float bj, float *cr, float *cj) {
	*cr = ar*br - aj*bj;
	*cj = aj*br + ar*bj;
}

static void decoder_reset(vdl2_channel_t *v) {
	v->decoder_state = DEC_PREAMBLE;
	bitstream_reset(v->bs);
	v->requested_bits = 4 * BPS + PREAMBLE_LEN;		// allow some extra room for leading zeros in xmtr ramp-up stage
}

static void demod_reset(vdl2_channel_t *v) {
	decoder_reset(v);
	v->bufe = v->bufs = v->sclk = 0;
	v->demod_state = DM_INIT;
	v->requested_samples = SYNC_SYMS * SPS;
	v->dm_phi = 0.f;
}

static void demod(vdl2_channel_t *v) {
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
		statsd_increment(v->freq, "demod.sync.good");
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

static void process_samples(vdl2_channel_t *v, float *sbuf, uint32_t len) {
	int i, available;
	float mag;
	float cwf, swf;
	v->samplenum = -1;
	for(i = 0; i < len;) {
#if DEBUG
		v->samplenum++;
#endif
		for(int k = INP_LPF_NPOLES; k > 0; k--) {
			   v->re[k] =    v->re[k-1];
			   v->im[k] =    v->im[k-1];
			v->lp_re[k] = v->lp_re[k-1];
			v->lp_im[k] = v->lp_im[k-1];
		}
		v->re[0] = sbuf[i++];
		v->im[0] = sbuf[i++];
// downmix
		if(v->offset_tuning) {
			sincosf_lut(v->dm_phi, &swf, &cwf);
			multiply(v->re[0], v->im[0], cwf, swf, &v->re[0], &v->im[0]);
			v->dm_phi += v->dm_dphi;
			v->dm_phi &= 0xffffff;
		}

// lowpass IIR
		v->lp_re[0] = chebyshev_lpf_2pole(v->re, v->lp_re);
		v->lp_im[0] = chebyshev_lpf_2pole(v->im, v->lp_im);
// decimation
		v->cnt %= v->oversample;
		if(v->cnt++ != 0)
			continue;

		mag = hypotf(v->lp_re[0], v->lp_im[0]);
		v->mag_lp = v->mag_lp * MAG_LP + mag * (1.0f - MAG_LP);
		v->nfcnt %= 1000;
// update noise floor estimate
		if(v->nfcnt++ == 0)
			v->mag_nf = NF_LP * v->mag_nf + (1.0f - NF_LP) * fminf(v->mag_lp, v->mag_nf) + 0.0001f;
		if(v->mag_lp > 3.0f * v->mag_nf) {
			if(v->demod_state == DM_IDLE)
				continue;
			if(v->sq == 0) {
				debug_print("*** on at (%d:%d) ***\n", v->bufnum, v->samplenum);
				v->sq = 1;
			}
		} else {
			if(v->sq == 1 && v->demod_state == DM_IDLE) {	// close squelch only when decoder finished work or errored
				// FIXME: time-limit this, because reading obvious trash does not make sense
				debug_print("*** off at (%d:%d) ***\n", v->bufnum, v->samplenum);
				v->sq = 0;
				demod_reset(v);
			}
		}
		if(v->sq == 1) {
			v->I[v->bufe] = v->lp_re[0];
			v->Q[v->bufe] = v->lp_im[0];
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
	v->bufnum++;
	if(DEBUG && v->bufnum % 10 == 0)
		debug_print("%u: noise_floor: %.1f dBFS\n", v->freq, 20.0f * log10f(v->mag_nf + 0.001f));
}

void process_buf_uchar(unsigned char *buf, uint32_t len, void *ctx) {
	if(len == 0) return;
	vdl2_state_t *v = (vdl2_state_t *)ctx;
	float *sbuf = v->sbuf;
	for(uint32_t i = 0; i < len; i++)
		sbuf[i] = levels[buf[i]];
	for(int i = 0; i < v->num_channels; i++)
		process_samples(v->channels[i], sbuf, len);
}

void process_buf_uchar_init() {
	levels = XCALLOC(256, sizeof(float));
	for (int i = 0; i < 256; i++) {
		levels[i] = (i-127.5f)/127.5f;
	}
}

void process_buf_short(unsigned char *buf, uint32_t len, void *ctx) {
	if(len == 0) return;
	vdl2_state_t *v = (vdl2_state_t *)ctx;
	float *sbuf = v->sbuf;
	int16_t *bbuf = (int16_t *)buf;
	len /= 2;
	for(uint32_t i = 0; i < len; i++)
		sbuf[i] = (float)bbuf[i] / 32768.0f;
	for(int i = 0; i < v->num_channels; i++)
		process_samples(v->channels[i], sbuf, len);
}

void input_lpf_init(uint32_t sample_rate) {
	assert(sample_rate != 0);
	chebyshev_lpf_init((float)INP_LPF_CUTOFF_FREQ / (float)sample_rate, INP_LPF_RIPPLE_PERCENT, INP_LPF_NPOLES, &A, &B);
}

void sincosf_lut_init() {
	for(uint32_t i = 0; i < 256; i++)
		sincosf(2.0f * M_PI * (float)i / 256.0f, sin_lut + i, cos_lut + i);
	sin_lut[256] = sin_lut[0];
	cos_lut[256] = cos_lut[0];
}

vdl2_channel_t *vdl2_channel_init(uint32_t centerfreq, uint32_t freq, uint32_t source_rate, uint32_t oversample) {
	vdl2_channel_t *v;
	v        = XCALLOC(1, sizeof(vdl2_channel_t));
	v->re    = XCALLOC(INP_LPF_NPOLES+1, sizeof(float));
	v->im    = XCALLOC(INP_LPF_NPOLES+1, sizeof(float));
	v->lp_re = XCALLOC(INP_LPF_NPOLES+1, sizeof(float));
	v->lp_im = XCALLOC(INP_LPF_NPOLES+1, sizeof(float));
	v->bs = bitstream_init(BSLEN);
	v->mag_nf = 2.0f;
// Cast to signed first, because casting negative float to uint is not portable
	v->dm_dphi = (uint32_t)(int)(((float)centerfreq - (float)freq) / (float)source_rate * 256.0f * 65536.0f);
	debug_print("dm_dphi: 0x%x\n", v->dm_dphi);
	v->offset_tuning = (centerfreq != freq);
	v->oversample = oversample;
	v->freq = freq;
	demod_reset(v);
	return v;
}
