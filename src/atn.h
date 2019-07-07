/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
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

#ifndef _ATN_H
#define _ATN_H 1
#include "dumpvdl2.h"			// dict, octet_string

#define ATN_TRAFFIC_TYPES_ALL 0x1f
#define ATSC_TRAFFIC_CLASSES_ALL 0xff
// atn.c
extern dict const atn_traffic_types[];
extern dict const atsc_traffic_classes[];

#endif // !_ATN_H
