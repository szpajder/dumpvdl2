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
#ifndef _KVARGS_H
#define _KVARGS_H 1

#include <stddef.h>         // ptrdiff_t

typedef struct kvargs_s kvargs;

typedef struct {
	kvargs *result;
	ptrdiff_t err_pos;
	int err;
} kvargs_parse_result;

kvargs *kvargs_new();
kvargs_parse_result kvargs_from_string(char *string);
char *kvargs_get(kvargs const *kv, char const *key);
char const *kvargs_get_errstr(int err);
void kvargs_destroy(kvargs *kv);

#endif // !_KVARGS_H
