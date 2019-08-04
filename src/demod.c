/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
 *  Copyright (c) 2017-2019 Tomasz Lemiech <szpajder@gmail.com>
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
#include <string.h>		// memset
#include <sys/time.h>		// gettimeofday
#include "config.h"
#ifdef HAVE_PTHREAD_BARRIERS
#include <pthread.h>		// pthread_barrier_wait
#else
#include "pthread_barrier.h"
#endif
#include "chebyshev.h"		// chebyshev_lpf_init
#include "dumpvdl2.h"

#define BSLEN 32768UL
#define PHERR_MAX 1000.f			// initial value for frame sync error (read: high)
#define SYNC_SKIP 3				// attempt frame sync every SYNC_SKIP samples (to reduce CPU usage)
#define SYNC_THRESHOLD 4.f			// assume we got frame sync if phase error is less than this threshold
#define ARITY 8
#define MAG_LP 0.9f
#define NF_LP 0.85f
// input lowpass filter design constants
#define INP_LPF_CUTOFF_FREQ 8000
#define INP_LPF_RIPPLE_PERCENT 0.5f
// do not change this; filtering routine is currently hardcoded to 2 poles to minimize CPU usage
#define INP_LPF_NPOLES 2

float *sbuf;
static float *levels;
static float sin_lut[257], cos_lut[257];
static uint32_t sbuf_len;
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
		lr_denom += (i - mean_X) * (i - mean_X);
	}
}

static float calc_para_vertex(float x, int d, float y1, float y2, float y3) {
	float denom = (float)(d * 2*d * (-d));
	float A = (x * (y2 - y1) + (x - d) * (y1 - y3) + (x - 2*d) * (y3 - y2)) / denom;
	float B = (x * x * (y1 - y2) + (x - d) * (x - d) * (y3 - y1) + (x - 2*d) * (x - 2*d) * (y2 - y3)) / denom;
	return(-B / (2 * A));
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
// Starting phase of pr_phase is 0. If we have a sync with a preamble starting with phase of 0, then
// errvec is a string of close-to-zero values. If the starting phase is different, then phase
// increments between symbols are still preserved, so errvec is a string of a constant value, which
// we subtract to get a string of zeros.
	for (int i = 0; i < PREAMBLE_SYMS; i++) {
		errvec[i] -= errvec_mean;
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
// We have passed the minimum value of the error metric and we are below
// the threshold, so we have a successful sync.
// Approximate the last three error-squared values with a parabola and locate its vertex,
// which is the sync point, from where we start the symbol clock.
		float vertex_x = calc_para_vertex(v->sclk, SYNC_SKIP, v->pherr[2], v->pherr[1], v->pherr[0]);
		v->sclk = -roundf(vertex_x);
// Save phase at the sync point (v->sclk is negative, ie pointing at the past sample)
		int sp = v->syncbufidx - v->sclk;
		if(sp < 0) sp += SYNC_BUFLEN;
		v->prev_phi = v->syncbuf[sp];
		v->dphi = v->prev_dphi;
		v->ppm_error = SYMBOL_RATE * v->dphi / (2.0f * M_PI * v->freq) * 1e+6;
		debug_print("Preamble found at %llu (pherr[2]=%f pherr[1]=%f pherr[0]=%f vertex_x=%f syncbufidx=%d, "
			"syncpoint=%d syncpoint_phase=%f sclk=%d v->dphi=%f ppm=%f)\n",
			v->samplenum - SYNC_SKIP, v->pherr[2], v->pherr[1], v->pherr[0], vertex_x, v->syncbufidx,
			sp, v->prev_phi, v->sclk, v->dphi, v->ppm_error);
		v->pherr[1] = v->pherr[2] = PHERR_MAX;
		return 1;
	}
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
	v->decoder_state = DEC_HEADER;
	v->requested_bits = HEADER_LEN;
	v->num_fec_corrections = 0;
	bitstream_reset(v->bs);
	bitstream_reset(v->frame_bs);
}

static void demod_reset(vdl2_channel_t *v) {
	decoder_reset(v);
	v->sclk = 0;
	v->demod_state = DM_INIT;
	v->pherr[1] = v->pherr[2] = PHERR_MAX;
	v->frame_pwr = 0.f;
	v->frame_pwr_cnt = 0;
}

static void demod(vdl2_channel_t *v, float re, float im) {
	static const uint8_t graycode[ARITY] = { 0, 1, 3, 2, 6, 7, 5, 4 };

	if(v->decoder_state == DEC_IDLE) {
		demod_reset(v);
	}

	switch(v->demod_state) {
	case DM_INIT:
		v->syncbufidx++; v->syncbufidx %= SYNC_BUFLEN;
		v->syncbuf[v->syncbufidx] = atan2(im, re);
		if(++v->sclk < SYNC_SKIP) {
			return;
		}
		v->sclk = 0;
// update noise floor estimate
		float mag = hypotf(re, im);
		v->mag_lp = v->mag_lp * MAG_LP + mag * (1.0f - MAG_LP);
		if(++v->nfcnt == 1000) {
			v->nfcnt = 0;
			v->mag_nf = NF_LP * v->mag_nf + (1.0f - NF_LP) * fminf(v->mag_lp, v->mag_nf) + 0.0001f;
		}
		if(got_sync(v)) {
			statsd_increment(v->freq, "demod.sync.good");
			gettimeofday(&v->burst_timestamp, NULL);
			v->demod_state = DM_SYNC;
			debug_print("DM_SYNC, v->sclk=%d\n", v->sclk);
		}
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
// update signal power average
		float symbol_pwr = re * re + im * im;
		v->frame_pwr = (v->frame_pwr * v->frame_pwr_cnt + symbol_pwr) / (v->frame_pwr_cnt + 1);
		v->frame_pwr_cnt++;

		debug_print("%llu: I: %f Q: %f symb_pwr: %f frame_pwr: %f dphi: %f * pi/4 idx: %d bits: %d\n",
			v->samplenum, re, im, symbol_pwr, v->frame_pwr, dphi, idx, graycode[idx]);

		v->prev_phi = phi;
		if(bitstream_append_msbfirst(v->bs, &(graycode[idx]), 1, BPS) < 0) {
			debug_print("%s", "bitstream_append_msbfirst failed\n");
			demod_reset(v);
			return;
		}
		if(v->bs->end - v->bs->start >= v->requested_bits) {
			debug_print("bitstream len=%u requested_bits=%u, launching frame decoder\n",
				v->bs->end - v->bs->start, v->requested_bits);
			decode_vdl_frame(v);
		}
		return;
	}
}

void *process_samples(void *arg) {
	int cnt = 0;
	float cwf, swf;
	float re[INP_LPF_NPOLES+1], im[INP_LPF_NPOLES+1];
	float lp_re[INP_LPF_NPOLES+1], lp_im[INP_LPF_NPOLES+1];
	CAST_PTR(v, vdl2_channel_t *, arg);
	v->samplenum = -1;
	memset(lp_re, 0, sizeof(lp_re));
	memset(lp_im, 0, sizeof(lp_im));
	memset(re, 0, sizeof(re));
	memset(im, 0, sizeof(im));
	while(1) {
		pthread_barrier_wait(&demods_ready);
		pthread_barrier_wait(&samples_ready);
		for(uint32_t i = 0; i < sbuf_len;) {
			for(int k = INP_LPF_NPOLES; k > 0; k--) {
				   re[k] =    re[k-1];
				   im[k] =    im[k-1];
				lp_re[k] = lp_re[k-1];
				lp_im[k] = lp_im[k-1];
			}
			re[0] = sbuf[i++];
			im[0] = sbuf[i++];
// downmix
			if(v->offset_tuning) {
				sincosf_lut(v->downmix_phi, &swf, &cwf);
				multiply(re[0], im[0], cwf, swf, &re[0], &im[0]);
				v->downmix_phi += v->downmix_dphi;
				v->downmix_phi &= 0xffffff;
			}
// lowpass IIR
			lp_re[0] = chebyshev_lpf_2pole(re, lp_re);
			lp_im[0] = chebyshev_lpf_2pole(im, lp_im);
// decimation
			if(++cnt == v->oversample) {
				cnt = 0;
#ifdef DEBUG
				v->samplenum++;
#endif
				demod(v, lp_re[0], lp_im[0]);
			}
		}
#ifdef DEBUG
		if(++v->bufnum == 10) {
			v->bufnum = 0;
			debug_print("%u: noise_floor: %.1f dBFS\n", v->freq, 20.0f * log10f(v->mag_nf + 0.001f));
		}
#endif
	}
}

void process_buf_uchar(unsigned char *buf, uint32_t len, void *ctx) {
	UNUSED(ctx);
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
	UNUSED(ctx);
	if(len == 0) return;
	CAST_PTR(bbuf, int16_t *, buf);
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
		SINCOSF(2.0f * M_PI * (float)i / 256.0f, sin_lut + i, cos_lut + i);
	sin_lut[256] = sin_lut[0];
	cos_lut[256] = cos_lut[0];
}

vdl2_channel_t *vdl2_channel_init(uint32_t centerfreq, uint32_t freq, uint32_t source_rate, uint32_t oversample) {
	NEW(vdl2_channel_t, v);
	v->bs = bitstream_init(BSLEN);
	v->frame_bs = bitstream_init(BSLEN);
	v->mag_nf = 2.0f;
// Cast to signed first, because casting negative float to uint is not portable
	v->downmix_dphi = (uint32_t)(int)(((float)centerfreq - (float)freq) / (float)source_rate * 256.0f * 65536.0f);
	debug_print("downmix_dphi: 0x%x\n", v->downmix_dphi);
	v->offset_tuning = (centerfreq != freq);
	v->oversample = oversample;
	v->freq = freq;
	demod_reset(v);
	return v;
}
