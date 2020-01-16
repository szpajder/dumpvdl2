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

typedef struct {
	char *registration;
	char *icaotypecode;
	char *operatorflagcode;
	char *manufacturer;
	char *type;
	char *registeredowners;
} ac_data_entry;

// ac_file.c
int ac_data_init(char const *bs_db_file);
void ac_data_destroy();
ac_data_entry *ac_data_entry_lookup(uint32_t addr);
