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

#ifndef _OUTPUT_FILE_H
#define _OUTPUT_FILE_H

#include "output-common.h"          // output_descriptor_t

// Maximum allowed length of a binary-serialized frame (including length field)
#define OUT_BINARY_FRAME_LEN_MAX       65536
// Size of the length field preceding binary-serialized frame
#define OUT_BINARY_FRAME_LEN_OCTETS    2

extern output_descriptor_t out_DEF_file;

#endif // !_OUTPUT_FILE_H
