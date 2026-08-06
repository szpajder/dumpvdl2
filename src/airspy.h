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

#ifndef _AIRSPY_H
#define _AIRSPY_H 1
#include <stdint.h>
#include "dumpvdl2.h"

// No Airspy sample rate is a multiple of SYMBOL_RATE * SPS, so the input
// stream always has to be converted. Decimating to 1050000 sps keeps the
// whole VDL2 band (136.700 - 137.000 MHz) within the resampled bandwidth.
#define AIRSPY_OVERSAMPLE 10
// Maximum number of devices to enumerate
#define AIRSPY_MAX_DEVICES 32
// Airspy gain settings are indices into the tuner's gain tables, not decibels,
// hence a separate "unset" marker instead of SDR_AUTO_GAIN.
#define AIRSPY_GAIN_UNSET -1
// Used when the user has not requested any particular gain distribution.
// Leaving every stage at 0 (which is how the device powers up) would make the
// receiver deaf.
#define AIRSPY_DEFAULT_LINEARITY_GAIN 16

// airspy.c
// NOTE: the airspy_* namespace belongs to libairspy (airspy_init() in
// particular), so the functions below are named to stay clear of it.
uint32_t airspy_open_device(char *dev, uint32_t sample_rate);
void airspy_start(vdl2_state_t *ctx, uint32_t centerfreq, int linearity_gain, int sensitivity_gain,
		int lna_gain, int mixer_gain, int vga_gain, int lna_agc, int mixer_agc,
		int correction, int biast, int packing);
void airspy_cancel();

#endif // !_AIRSPY_H
