/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017 Fabrice Crohas <fcrohas@gmail.com>
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
#include <stdint.h>                         // uint32_t
#include "dumpvdl2.h"                       // vdl2_state_t
#define SDRPLAY_OVERSAMPLE               20

void sdrplay_init(vdl2_state_t const *ctx, char const *dev, uint32_t sample_rate, char const *antenna,
		uint32_t freq, int gr, int ppm_error, int enable_biast,
		int enable_notch_filter, int enable_agc, int tuner);
void sdrplay_cancel();
