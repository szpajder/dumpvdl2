/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2020 Tomasz Lemiech <szpajder@gmail.com>
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
#include "dumpvdl2.h"			// vdl2_state_t

#define SOAPYSDR_BUFSIZE (32*16384)
#define SOAPYSDR_BUFCNT 15
#define SOAPYSDR_OVERSAMPLE 20
#define SOAPYSDR_SAMPLE_PER_BUFFER 65536
#define SOAPYSDR_RATE (SYMBOL_RATE * SPS * SOAPYSDR_OVERSAMPLE)

// soapysdr.c
void soapysdr_init(vdl2_state_t *ctx, char *dev, char *antenna, int freq,
	float gain, int correction, char *settings, char *gains);
void soapysdr_cancel();
