/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2023 Tomasz Lemiech <szpajder@gmail.com>
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
#include <stdint.h>
#include "dumpvdl2.h"
#define MIRISDR_BUFSIZE 320000
#define MIRISDR_BUFCNT 32
#define MIRISDR_OVERSAMPLE 13
#define MIRISDR_RATE (SYMBOL_RATE * SPS * MIRISDR_OVERSAMPLE)

// mirics.c
void mirisdr_init(vdl2_state_t *ctx, char *dev, int flavour, uint32_t freq, float gain,
		int freq_offset, int usb_xfer_mode);
void mirisdr_cancel();
