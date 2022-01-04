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

#include <stdint.h>

#define AP_INFO_BUF_SIZE 128

typedef struct {
	char *ap_name;
	char *ap_city;
	char *ap_country;
	char *ap_icao_code;
	double ap_lat;
	double ap_lon;
} ap_data_entry;

// ap_data.c
int ap_data_init(char const *ap_db_file);
void ap_data_destroy();
ap_data_entry *ap_data_entry_lookup(char *ap_icao);
