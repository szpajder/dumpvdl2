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

#ifndef _RESAMPLER_H
#define _RESAMPLER_H 1
#include <stdint.h>

// Rational (L/M) polyphase resampler for interleaved complex float samples.
// All buffer lengths are expressed in floats (ie. twice the number of complex
// samples), to match the layout of the sample buffers used everywhere else.

typedef struct resampler_s resampler_t;

resampler_t *resampler_init(uint32_t input_rate, uint32_t output_rate);
uint32_t resampler_output_len_max(resampler_t const *r, uint32_t input_len);
uint32_t resampler_process(resampler_t *r, float const *in, uint32_t input_len, float *out);
void resampler_stats(resampler_t const *r, uint32_t *interp, uint32_t *decim, uint32_t *taps_per_phase);
void resampler_destroy(resampler_t *r);

#endif // !_RESAMPLER_H
