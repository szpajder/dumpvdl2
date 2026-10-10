/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2026 Tomasz Lemiech <szpajder@gmail.com>
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
#include <math.h>               // sin(), sqrt(), fabs(), fmax()
#include <stdint.h>
#include <stdio.h>              // fprintf()
#include <string.h>             // memcpy()
#include "dumpvdl2.h"           // XCALLOC(), XREALLOC(), XFREE(), NEW(), ASSERT(), debug_print()
#include "resampler.h"

// Length of the prototype filter, expressed as a number of taps per period of
// the lower of the two sample rates. This sets the width of the filter
// transition band to roughly (6 / PROTO_TAPS_PER_PERIOD) of the lower Nyquist
// frequency, ie. with the value below the response is flat up to 0.8 * Nyquist
// and the stopband starts at 1.2 * Nyquist. Signals located in the outermost
// 20% of the resampled band are therefore not guaranteed to be alias-free -
// increase the oversampling factor if the channels of interest land there.
#define PROTO_TAPS_PER_PERIOD 32
// Kaiser window beta for ~70 dB of stopband attenuation - well below the
// noise floor of any SDR which is likely to be used with this program.
#define KAISER_BETA 6.75
// Sanity limits. The interpolation factor is the numerator of
// output_rate/input_rate in its lowest terms, so it only grows large when the
// two rates are not simple fractions of each other.
#define MAX_INTERP_FACTOR 8192U
#define MAX_PROTO_TAPS (1U << 20)

struct resampler_s {
	float *taps;                    // prototype filter in polyphase order: taps[phase * taps_per_phase + k]
	float *hist;                    // tail of the previous input block (taps_per_phase - 1 complex samples)
	float *work;                    // hist ++ current input block
	uint32_t work_size;             // allocated length of work (in floats)
	uint32_t hist_len;              // length of hist (in floats)
	uint32_t taps_per_phase;
	uint32_t L, M;                  // interpolation / decimation factor
	uint32_t phase;                 // polyphase branch to use for the next output sample
	uint32_t skip;                  // input samples to skip at the start of the next block
};

static uint32_t gcd_u32(uint32_t a, uint32_t b) {
	while(b != 0) {
		uint32_t tmp = a % b;
		a = b;
		b = tmp;
	}
	return a;
}

// Modified Bessel function of the first kind, order 0
static double bessel_i0(double x) {
	double sum = 1.0, term = 1.0;
	for(int i = 1; i < 64; i++) {
		double t = x / (2.0 * (double)i);
		term *= t * t;
		sum += term;
		if(term < sum * 1e-12) {
			break;
		}
	}
	return sum;
}

// Designs a Kaiser-windowed sinc lowpass filter of ntaps taps, to be run at
// the interpolated (L * input_rate) sample rate. Its cutoff frequency is the
// lower of the two Nyquist frequencies involved, so that the same filter
// serves as both the interpolation and the decimation filter.
static float *design_prototype_lpf(uint32_t ntaps, uint32_t L, uint32_t M) {
	ASSERT(ntaps > 1);
	float *h = XCALLOC(ntaps, sizeof(float));
	double const fc = 0.5 / (double)(L > M ? L : M);
	double const i0_beta = bessel_i0(KAISER_BETA);
	double const center = (double)(ntaps - 1) / 2.0;
	double sum = 0.0;
	for(uint32_t i = 0; i < ntaps; i++) {
		double t = (double)i - center;
		double sinc = fabs(t) < 1e-9 ? 2.0 * fc : sin(2.0 * M_PI * fc * t) / (M_PI * t);
		double r = t / center;
		double w = bessel_i0(KAISER_BETA * sqrt(fmax(0.0, 1.0 - r * r))) / i0_beta;
		h[i] = (float)(sinc * w);
		sum += sinc * w;
	}
	// Normalize to unity passband gain. The filter operates on a stream which
	// has been zero-stuffed by a factor of L, hence the gain of L.
	double scale = (double)L / sum;
	for(uint32_t i = 0; i < ntaps; i++) {
		h[i] *= (float)scale;
	}
	return h;
}

resampler_t *resampler_init(uint32_t input_rate, uint32_t output_rate) {
	if(input_rate == 0 || output_rate == 0) {
		fprintf(stderr, "resampler: sample rates must be non-zero\n");
		return NULL;
	}
	uint32_t g = gcd_u32(input_rate, output_rate);
	uint32_t L = output_rate / g;
	uint32_t M = input_rate / g;
	if(L > MAX_INTERP_FACTOR) {
		fprintf(stderr, "resampler: cannot convert %u to %u sps: interpolation factor %u is too large "
				"(max %u). Pick a sample rate which is a simpler fraction of the working rate, "
				"or use --resampler interp.\n", input_rate, output_rate, L, MAX_INTERP_FACTOR);
		return NULL;
	}
	// Make the prototype filter span PROTO_TAPS_PER_PERIOD periods of the lower
	// of the two sample rates, rounded up so that it splits evenly into L
	// polyphase branches.
	uint64_t taps_per_phase = ((uint64_t)PROTO_TAPS_PER_PERIOD * (L > M ? L : M) + L - 1) / L;
	if(taps_per_phase * L > MAX_PROTO_TAPS) {
		fprintf(stderr, "resampler: cannot convert %u to %u sps: filter would need %lu taps "
				"(max %u)\n", input_rate, output_rate, (unsigned long)(taps_per_phase * L), MAX_PROTO_TAPS);
		return NULL;
	}
	NEW(resampler_t, r);
	r->L = L;
	r->M = M;
	r->taps_per_phase = (uint32_t)taps_per_phase;
	uint32_t ntaps = r->taps_per_phase * L;
	float *proto = design_prototype_lpf(ntaps, L, M);
	// Rearrange the prototype into polyphase branches. Output sample n uses
	// branch (n * M) mod L, which holds taps proto[phase], proto[phase + L], ...
	r->taps = XCALLOC(ntaps, sizeof(float));
	for(uint32_t phase = 0; phase < L; phase++) {
		for(uint32_t k = 0; k < r->taps_per_phase; k++) {
			r->taps[phase * r->taps_per_phase + k] = proto[phase + k * L];
		}
	}
	XFREE(proto);
	r->hist_len = 2 * (r->taps_per_phase - 1);
	r->hist = XCALLOC(r->hist_len, sizeof(float));
	debug_print(D_DEMOD, "resampler: %u -> %u sps, L=%u M=%u, %u taps/phase (%u total)\n",
			input_rate, output_rate, L, M, r->taps_per_phase, ntaps);
	return r;
}

void resampler_stats(resampler_t const *r, uint32_t *interp, uint32_t *decim, uint32_t *taps_per_phase) {
	ASSERT(r != NULL);
	if(interp != NULL) {
		*interp = r->L;
	}
	if(decim != NULL) {
		*decim = r->M;
	}
	if(taps_per_phase != NULL) {
		*taps_per_phase = r->taps_per_phase;
	}
}

uint32_t resampler_output_len_max(resampler_t const *r, uint32_t input_len) {
	ASSERT(r != NULL);
	// Every output sample advances the input by at least M/L samples. Add one
	// for the output which may still be pending from the previous block.
	uint64_t samples = (uint64_t)(input_len / 2) * r->L / r->M + 1;
	return (uint32_t)(2 * samples);
}

// Resamples one block of interleaved complex samples. Returns the number of
// floats written to out, which must have room for resampler_output_len_max()
// of them.
uint32_t resampler_process(resampler_t *r, float const *in, uint32_t input_len, float *out) {
	ASSERT(r != NULL);
	ASSERT(in != NULL);
	ASSERT(out != NULL);
	ASSERT(input_len % 2 == 0);

	uint32_t total = r->hist_len + input_len;
	if(total > r->work_size) {
		r->work = XREALLOC(r->work, total * sizeof(float));
		r->work_size = total;
	}
	memcpy(r->work, r->hist, r->hist_len * sizeof(float));
	memcpy(r->work + r->hist_len, in, input_len * sizeof(float));

	uint32_t const tpp = r->taps_per_phase;
	uint32_t const total_samples = total / 2;
	// Index of the newest sample taking part in the next dot product. The
	// oldest one is idx - (tpp - 1), which is why the previous block's tail is
	// prepended to the work buffer.
	uint32_t idx = (tpp - 1) + r->skip;
	uint32_t out_len = 0;
	while(idx < total_samples) {
		float const *h = r->taps + r->phase * tpp;
		float const *x = r->work + 2 * idx;
		float acc_re = 0.f, acc_im = 0.f;
		for(uint32_t k = 0; k < tpp; k++) {
			acc_re += h[k] * x[0];
			acc_im += h[k] * x[1];
			x -= 2;
		}
		out[out_len++] = acc_re;
		out[out_len++] = acc_im;
		r->phase += r->M;
		idx += r->phase / r->L;
		r->phase %= r->L;
	}
	r->skip = idx - total_samples;
	memcpy(r->hist, r->work + (total - r->hist_len), r->hist_len * sizeof(float));
	return out_len;
}

void resampler_destroy(resampler_t *r) {
	if(r == NULL) {
		return;
	}
	XFREE(r->taps);
	XFREE(r->hist);
	XFREE(r->work);
	XFREE(r);
}
