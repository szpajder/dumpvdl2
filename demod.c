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
#include <pthread.h>		// pthread_barrier_wait
#include "chebyshev.h"		// chebyshev_lpf_init
#include "dumpvdl2.h"

// FIXME: temp
#include <unistd.h>

float *sbuf;
static float *levels;
static float sin_lut[257], cos_lut[257];
static uint32_t sbuf_len;

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

static float lr_X[PREAMBLE_SYMS];
static float lr_denom;

void demod_sync_init() {
// pre-compute linear regression constants
	float mean_X = 0.f;
	lr_denom = 0.f;
	for(int i = 0; i < PREAMBLE_SYMS; i++) {
		mean_X += i;
	}
	mean_X /= PREAMBLE_SYMS;
	for(int i = 0; i < PREAMBLE_SYMS; i++) {
		lr_X[i] = i - mean_X;
		debug_print("lr_X[%d]=%f\n", i, lr_X[i]);
		lr_denom += (i - mean_X) * (i - mean_X);
	}
	debug_print("lr_denom=%f\n", lr_denom);
}

static float calc_para_vertex(float x, int d, float y1, float y2, float y3) {
// FIXME: static const?
	float denom = (float)(d * 2*d * (-d));
	float A = (x * (y2 - y1) + (x-d) * (y1 - y3) + (x-2*d) * (y3 - y2)) / denom;
	float B = (x * x * (y1 - y2) + (x-d)*(x-d) * (y3 - y1) + (x-2*d)*(x-2*d) * (y2 - y3)) / denom;
	return(-B / (2*A));
}

static int got_sync(vdl2_channel_t *v) {
// Cumulative phase after each symbol of VDL2 preamble, wrapped to (-pi; pi> range
	static const float pr_phase[PREAMBLE_SYMS] = {
		 0 * M_PI / 4,
		 3 * M_PI / 4,
		-3 * M_PI / 4,
		 1 * M_PI / 4,
		 1 * M_PI / 4,
		 2 * M_PI / 4,
		 0 * M_PI / 4,
		 4 * M_PI / 4,
		-3 * M_PI / 4,
		 4 * M_PI / 4,
		-2 * M_PI / 4,
		 3 * M_PI / 4,
		 1 * M_PI / 4,
		-2 * M_PI / 4,
		-3 * M_PI / 4,
		 0 * M_PI / 4
	};
// v->syncbuf stores phases (complex arguments) of previous PREAMBLE_SYMS * SPS samples.
// v->syncbufidx is the position of the last stored sample in the vector.
// Compute sync error as a vector of differences between each symbol phase and the expected phase of
// the respective preamble symbol.
	float errvec[PREAMBLE_SYMS];
	float errvec_mean = 0.f, unwrap = 0.f;
	float prev_err = errvec_mean = errvec[0] = v->syncbuf[(v->syncbufidx + SPS) % SYNC_BUFLEN] - pr_phase[0];
	debug_print("v->syncbufidx=%d, sync start is at %d\n", v->syncbufidx, (v->syncbufidx + SPS) % SYNC_BUFLEN);
	for(int i = 1; i < PREAMBLE_SYMS; i++) {
		float cur_err = v->syncbuf[(v->syncbufidx + (i + 1) * SPS) % SYNC_BUFLEN] - pr_phase[i];
		float errdiff = cur_err - prev_err;
		prev_err = cur_err;
// Remove phase jumps larger than M_PI
		if(errdiff > M_PI) {
			unwrap -= 2.0f * M_PI;
		} else if(errdiff < -M_PI) {
			unwrap += 2.0f * M_PI;
		}
		errvec[i] = cur_err + unwrap;
		errvec_mean += errvec[i];
	}
	errvec_mean /= PREAMBLE_SYMS;
	debug_print("errvec_mean: %f\n", errvec_mean);
// Starting phase of pr_phase is 0. If we have a sync with a preamble starting with phase of 0, then
// errvec is a string of close-to-zero values. If the starting phase is different, then phase
// increments between symbols are still preserved, so errvec is a string of a constant value, which
// we subtract to get a string of zeros.
	for (int i = 0; i < PREAMBLE_SYMS; i++) {
		errvec[i] -= errvec_mean;
//		debug_print("errvec[%d]=%f\n", i, errvec[i]);
	}
// If there is a non-zero frequency offset between transmitter and receiver, then errvec values
// are not constant, but increasing or decreasing monotonically.
// In order to estimate this error, we apply a linear regression to errvec, according to:
// y=Ax+B
// A = sum((x(i) - mean(x)) * (y(i) - mean(y))) / sum( (x(i) - mean(x))^2 )
// lr_X = x(i) - mean(x) and lr_denom = sum( (x(i) - mean(x))^2 ) are precomputed in demod_sync_init().
	float freq_err = 0.f;
	for(int i = 0; i < PREAMBLE_SYMS; i++) {
		freq_err += lr_X[i] * errvec[i];
	}
	freq_err /= lr_denom;
// Compute new error vector with frequency correction applied
// and the overall frame sync error value
	float err = 0.f;
	v->pherr[0] = 0.f;
	for(int i = 0; i < PREAMBLE_SYMS; i++) {
		err = errvec[i] - freq_err * lr_X[i];
		v->pherr[0] += err * err;
	}

	if (v->pherr[1] < SYNC_THRESHOLD && v->pherr[0] > v->pherr[1]) {
// We have passed the minimum value of the error metric.
// Approximate the last three points with a parabola and locate its vertex,
// which is the sync point, from where we start the symbol clock.
		float vertex_x = calc_para_vertex(v->sclk, SYNC_SKIP, v->pherr[2], v->pherr[1], v->pherr[0]);
		v->sclk = -roundf(vertex_x);
// Save phase at the sync point (v->sclk is negative, ie pointing at the past sample)
		int sp = v->syncbufidx - v->sclk;
		if(sp < 0) sp += SYNC_BUFLEN;
		v->prev_phi = v->syncbuf[sp];
//		v->dphi = freq_err;	// FIXME: v->prev_dphi
		v->dphi = v->prev_dphi;
		v->ppm_error = SYMBOL_RATE * v->dphi / (2.0f * M_PI * v->freq) * 1e+6;
		debug_print("Preamble found at %lu (prev2_pherr=%f prev_pherr=%f cur_pherr=%f vertex_x=%f syncbufidx=%d, "
			"syncpoint=%d syncpoint_phase=%f sclk=%d freq_err=%f prev_freq_err=%f ppm=%f)\n",
			v->samplenum - SYNC_SKIP, v->pherr[2], v->pherr[1], v->pherr[0], vertex_x, v->syncbufidx,
			sp, v->prev_phi, v->sclk, freq_err, v->prev_dphi, v->ppm_error);
		v->pherr[1] = v->pherr[2] = PHERR_MAX;
		return 1;
	}
	debug_print("%lu: v->pherr[1]=%f v->pherr[0]=%f\n", v->samplenum, v->pherr[1], v->pherr[0]);
	v->pherr[2] = v->pherr[1];
	v->pherr[1] = v->pherr[0];
	v->prev_dphi = freq_err;
	return 0;
}


static void multiply(float ar, float aj, float br, float bj, float *cr, float *cj) {
	*cr = ar*br - aj*bj;
	*cj = aj*br + ar*bj;
}

static void decoder_reset(vdl2_channel_t *v) {
//	v->decoder_state = DEC_PREAMBLE;
	v->decoder_state = DEC_HEADER;
	bitstream_reset(v->bs);
//	v->requested_bits = 4 * BPS + PREAMBLE_LEN;		// allow some extra room for leading zeros in xmtr ramp-up stage
	v->requested_bits = HEADER_LEN;
}

static void demod_reset(vdl2_channel_t *v) {
	decoder_reset(v);
	v->sclk = 0;
	v->demod_state = DM_INIT;
// FIXME: ?
//	v->dm_phi = 0.f;
	v->pherr[1] = v->pherr[2] = PHERR_MAX;
}

static void demod(vdl2_channel_t *v, float re, float im) {
	static const uint8_t graycode[ARITY] = { 0, 1, 3, 2, 6, 7, 5, 4 };

	if(v->decoder_state == DEC_IDLE) {
		debug_print("%s", "demod: decoder_state is DEC_IDLE, resetting demodulator\n");
		demod_reset(v);
//		return;
	}

	switch(v->demod_state) {
	case DM_INIT:
		v->syncbufidx++; v->syncbufidx %= SYNC_BUFLEN;
		v->syncbuf[v->syncbufidx] = atan2(im, re);
//		debug_print("re=%f im=%f v->syncbuf[%d] = %f\n", re, im, v->syncbufidx, v->syncbuf[v->syncbufidx]);
		if(++v->sclk < SYNC_SKIP) {
			return;
		}
		v->sclk = 0;
		if(!got_sync(v)) {
			return;
		}
		statsd_increment(v->freq, "demod.sync.good");
		v->demod_state = DM_SYNC;
		debug_print("DM_SYNC, v->sclk=%d\n", v->sclk);
		return;
	case DM_SYNC:
		if(++v->sclk < SPS) {
			return;
		}
		v->sclk = 0;
		float phi = atan2(im, re);
		float dphi = phi - v->prev_phi - v->dphi;
		if(dphi < 0) {
			dphi += 2.0f * M_PI;
		} else if(dphi > 2.0f * M_PI) {
			dphi -= 2.0f * M_PI;
		}
		dphi /= M_PI_4;
		int idx = (int)roundf(dphi) % ARITY;
		debug_print("%lu: I: %f Q: %f dphi: %f * pi/4 idx: %d bits: %d\n",
			v->samplenum, re, im, dphi, idx, graycode[idx]);
		v->prev_phi = phi;
		if(bitstream_append_msbfirst(v->bs, &(graycode[idx]), 1, BPS) < 0) {
			debug_print("%s", "bitstream_append_msbfirst failed\n");
			demod_reset(v);
			return;
		}
		if(v->bs->end - v->bs->start >= v->requested_bits) {
			debug_print("bitstream len=%u requested_bits=%u, launching frame decoder\n", v->bs->end - v->bs->start, v->requested_bits);
			decode_vdl_frame(v);
			if(v->decoder_state == DEC_IDLE) {	// decoding finished or failed
				v->demod_state = DM_IDLE;	// FIXME: remove this state
			}
		}
		return;
	case DM_IDLE:
		return;
	}
}

void *process_samples(void *arg) {
	int i;
	float mag;
	float cwf, swf;
	vdl2_channel_t *v = (vdl2_channel_t *)arg;
	v->samplenum = -1;
	while(1) {
		pthread_barrier_wait(&demods_ready);
		pthread_barrier_wait(&samples_ready);
		for(i = 0; i < sbuf_len;) {
			for(int k = INP_LPF_NPOLES; k > 0; k--) {
// FIXME: no need to have this in v
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
#if DEBUG
			v->samplenum++;
#endif

			mag = hypotf(v->lp_re[0], v->lp_im[0]);
			v->mag_lp = v->mag_lp * MAG_LP + mag * (1.0f - MAG_LP);
			v->nfcnt %= 1000;
// update noise floor estimate
// TODO: update NF only when demod_state is DM_INIT (ie. outside of a frame)
			if(v->nfcnt++ == 0)
				v->mag_nf = NF_LP * v->mag_nf + (1.0f - NF_LP) * fminf(v->mag_lp, v->mag_nf) + 0.0001f;
			demod(v, v->lp_re[0], v->lp_im[0]);
		}
		v->bufnum++;
		if(DEBUG && v->bufnum % 10 == 0)
			debug_print("%u: noise_floor: %.1f dBFS\n", v->freq, 20.0f * log10f(v->mag_nf + 0.001f));
	}
}

void process_buf_uchar(unsigned char *buf, uint32_t len, void *ctx) {
	if(len == 0) return;
	pthread_barrier_wait(&demods_ready);
	sbuf_len = len;
	for(uint32_t i = 0; i < sbuf_len; i++)
		sbuf[i] = levels[buf[i]];
	pthread_barrier_wait(&samples_ready);
}

void process_buf_uchar_init() {
	levels = XCALLOC(256, sizeof(float));
	for (int i = 0; i < 256; i++) {
		levels[i] = (i-127.5f)/127.5f;
	}
}

void process_buf_short(unsigned char *buf, uint32_t len, void *ctx) {
	if(len == 0) return;
	int16_t *bbuf = (int16_t *)buf;
	pthread_barrier_wait(&demods_ready);
	sbuf_len = len / 2;
	for(uint32_t i = 0; i < sbuf_len; i++)
		sbuf[i] = (float)bbuf[i] / 32768.0f;
	pthread_barrier_wait(&samples_ready);
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
