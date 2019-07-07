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
#include "dumpvdl2.h"			// dict

dict const atn_traffic_types[] = {
	{ .id =  1, .val = "ATS" },
	{ .id =  2, .val = "AOC" },
	{ .id =  4, .val = "ATN Administrative" },
	{ .id =  8, .val = "General Comms" },
	{ .id = 16, .val = "ATN System Mgmt" },
	{ .id =  0, .val = NULL }
};

dict const atsc_traffic_classes[] = {
	{ .id =  1, .val = "A" },
	{ .id =  2, .val = "B" },
	{ .id =  4, .val = "C" },
	{ .id =  8, .val = "D" },
	{ .id = 16, .val = "E" },
	{ .id = 32, .val = "F" },
	{ .id = 64, .val = "G" },
	{ .id =128, .val = "H" },
	{ .id =  0, .val = NULL }
};
