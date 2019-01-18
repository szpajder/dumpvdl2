/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
 *  Copyright (c) 2017 Fabrice Crohas <fcrohas@gmail.com>
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
#include <stdint.h>				// uint32_t
#include "dumpvdl2.h"				// vdl2_state_t
#define MAX_IF_GR		59		// Upper limit of IF GR
#define MIN_IF_GR		20		// Lower limit of IF GR (in normal IF GR range)
#define MIXER_GR		19
#define DEFAULT_AGC_SETPOINT	-35
#define ASYNC_BUF_NUMBER	15
#define ASYNC_BUF_SIZE	 	(32*16384)	// 512k shorts
#define SDRPLAY_OVERSAMPLE	20
#define SDRPLAY_RATE (SYMBOL_RATE * SPS * SDRPLAY_OVERSAMPLE)

typedef struct {
	void *context;
	unsigned char *sdrplay_data;
	int data_index;
} sdrplay_ctx_t;

typedef enum {
	HW_UNKNOWN	= 0,
	HW_RSP1		= 1,
	HW_RSP2		= 2,
	HW_RSP1A	= 3,
	HW_RSPDUO	= 4
} sdrplay_hw_type;
#define NUM_HW_TYPES 5

void sdrplay_init(vdl2_state_t * const ctx, char * const dev, char * const antenna,
	uint32_t const freq, int const gr, int const ppm_error,	int const enable_biast,
	int const enable_notch_filter, int enable_agc, int tuner);
void sdrplay_cancel();
