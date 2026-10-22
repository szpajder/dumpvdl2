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

#ifndef _AIRSPYHF_H
#define _AIRSPYHF_H 1
#include <stdint.h>
#include "dumpvdl2.h"

// No Airspy HF+ sample rate is a multiple of SYMBOL_RATE * SPS, so the input
// stream always has to be converted. The device tops out well below the other
// Airspys (768 kHz on a Discovery), which puts a low ceiling on the working
// rate - it has to stay at or below whatever the device delivers, because the
// input resampler only ever decimates. Decimating to 630000 sps leaves the
// whole VDL2 band (136.700 - 137.000 MHz) inside the alias-free part of the
// resampled bandwidth, and still fits under the lowest rate a Discovery is
// likely to be run at.
#define AIRSPYHF_OVERSAMPLE 6
// Maximum number of devices to enumerate
#define AIRSPYHF_MAX_DEVICES 32
// Attenuator setting is an index into 6 dB steps, not a value in decibels,
// hence a separate "unset" marker instead of SDR_AUTO_GAIN.
#define AIRSPYHF_ATT_UNSET -1
#define AIRSPYHF_ATT_MAX 8
// The AGC defaults to on, but "on" and "off" are both meaningful values, so
// "not given on the command line" needs a marker of its own.
#define AIRSPYHF_AGC_UNSET -1

// airspyhf.c
// NOTE: libairspyhf owns the whole airspyhf_* namespace, and unlike libairspy
// it uses the obvious names for the obvious things - airspyhf_start() is the
// library's own streaming entry point. The functions below are therefore
// prefixed with input_ rather than following the <driver>_start() convention
// used by the other SDR drivers here.
uint32_t input_airspyhf_open_device(char *dev, uint32_t sample_rate);
void input_airspyhf_start(vdl2_state_t *ctx, uint32_t centerfreq, int agc, int agc_threshold,
		int att, int lna, int correction, int biast);
void input_airspyhf_cancel();

#endif // !_AIRSPYHF_H
