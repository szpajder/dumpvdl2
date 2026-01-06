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

#define _GNU_SOURCE         // for sincosf
#include <assert.h>         // assert
#include <math.h>           // expf, sincosf, sinf, tanf, sqrtf, powf, M_PI
#include <stdlib.h>         // calloc
#include <string.h>         // memset
#include "config.h"         // SINCOSF
#include "chebyshev.h"      // MAX_POLES, MAX_RIPPLE
#include "dumpvdl2.h"       // debug_print, XCALLOC
#define LP_BSIZE (MAX_POLES + 3)

// Based on "The Scientist and Engineer's Guide to Digital Signal Processing"
// Steven W. Smith, Ph.D.
static void chebyshev_lpf_calc_pole(int p, float cutoff_freq, float ripple,
		int npoles, float *AA, float *BB) {
	float rp, ip;
	SINCOSF(M_PI/(2 * npoles) + (p-1) * M_PI / npoles, &ip, &rp);
	rp = -rp;
	if(ripple != 0.f) {
		float es = sqrtf(powf(100.f / (100.f - ripple), 2.f) - 1.f);
		float vx = (1.f / npoles) * logf((1.f/es) + sqrtf(1.f/(es*es) + 1.f));
		float kx = (1.f / npoles) * logf((1.f/es) + sqrtf(1.f/(es*es) - 1.f));
		kx = (expf(kx) + expf(-kx)) / 2.f;
		rp *= ((expf(vx) - expf(-vx)) / 2.f) / kx;
		ip *= ((expf(vx) + expf(-vx)) / 2.f) / kx;
		debug_print(D_MISC, "es=%f, vx=%f, kx=%f\n", es, vx, kx);
	}
	debug_print(D_MISC, "rp=%f ip=%f\n", rp, ip);
	float t = 2.f * tanf(0.5f);
	float w = 2.f * M_PI * cutoff_freq;
	float m = rp * rp + ip * ip;
	float d = 4.f - 4.f * rp * t + m * t * t;
	float x0 = t * t / d;
	float x1 = 2.f * x0;
	float x2 = x0;
	float y1 = (8.f - 2.f * m * t * t) / d;
	float y2 = (-4.f - 4.f * rp * t - m * t * t) / d;
	debug_print(D_MISC, "t=%f w=%f m=%f d=%f\n", t, w, m, d);
	debug_print(D_MISC, "x0=%f x1=%f x2=%f y1=%f y2=%f\n", x0, x1, x2, y1, y2);
	float k = sinf(0.5f - w / 2.f) / sinf(0.5f + w / 2.f);
	d = 1 + y1 * k - y2 * k * k;
	AA[0] = (x0 - x1 * k + x2 * k * k) / d;
	AA[1] = (-2.f * x0 * k + x1 + x1 * k * k - 2.f * x2 * k) / d;
	AA[2] = (x0 * k * k - x1 * k + x2) / d;
	BB[1] = (2.f * k + y1 + y1 * k * k - 2.f * y2 * k) / d;
	BB[2] = (-(k * k) - y1 * k + y2) / d;
}

void chebyshev_lpf_init(float cutoff_freq, float ripple, int npoles,
		float **Aptr, float **Bptr) {
	assert(npoles > 0);
	assert(npoles <= MAX_POLES);
	assert((npoles & 1) == 0);
	assert(cutoff_freq >= 0.f);
	assert(cutoff_freq <= 0.5f);
	assert(ripple >= 0.f);
	assert(ripple <= MAX_RIPPLE);

	*Aptr = XCALLOC(LP_BSIZE, sizeof(float));
	*Bptr = XCALLOC(LP_BSIZE, sizeof(float));
	float TA[LP_BSIZE], TB[LP_BSIZE];
	float AA[3], BB[3];
	memset(TA, 0, LP_BSIZE * sizeof(float));
	memset(TB, 0, LP_BSIZE * sizeof(float));
	memset(AA, 0, 3 * sizeof(float));
	memset(BB, 0, 3 * sizeof(float));
	float *A = *Aptr;
	float *B = *Bptr;
	A[2] = 1.f; B[2] = 1.f;

	for(int p = 1; p <= npoles / 2; p++) {
		chebyshev_lpf_calc_pole(p, cutoff_freq, ripple, npoles, AA, BB);
		debug_print(D_MISC, "AA[0] = %f\n", AA[0]);
		for(int i = 1; i < 3; i++)
			debug_print(D_MISC, "AA[%d] = %f\tBB[%d] = %f\n", i, AA[i], i, BB[i]);
		memcpy(TA, A, LP_BSIZE * sizeof(float));
		memcpy(TB, B, LP_BSIZE * sizeof(float));
		for(int i = 2; i < LP_BSIZE; i++) {
			A[i] = AA[0] * TA[i] + AA[1] * TA[i-1] + AA[2] * TA[i-2];
			B[i] =         TB[i] - BB[1] * TB[i-1] - BB[2] * TB[i-2];
		}
	}
	B[2] = 0.f;
	for(int i = 0; i < LP_BSIZE-2; i++) {
		A[i] = A[i+2];
		B[i] = -B[i+2];
	}
	float sa = 0.f, sb = 0.f;
	for(int i = 0; i < LP_BSIZE-2; i++) {
		sa += A[i];
		sb += B[i];
	}
	float gain = sa / (1.f - sb);
	for(int i = 0; i < LP_BSIZE-2; i++) {
		A[i] /= gain;
	}
	debug_print(D_MISC, "a%d = %.12f\n", 0, A[0]);
	for(int i = 1; i <= npoles; i++) {
		debug_print(D_MISC, "a%d = %.12f\tb%d = %.12f\n", i, A[i], i, B[i]);
	}
}
