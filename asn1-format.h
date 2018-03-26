/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2018 Tomasz Lemiech <szpajder@gmail.com>
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

#include "asn1/constr_TYPE.h"		// asn_TYPE_descriptor_t

typedef struct {
	asn_TYPE_descriptor_t *type;
	void (*format)(FILE *, char const * const label, asn_TYPE_descriptor_t *, const void *, int);
	char const * const label;
} asn_formatter_t;

#define ASN1_FORMATTER_PROTOTYPE(x) static void x(FILE *stream, char const * const label, asn_TYPE_descriptor_t *td, void const *sptr, int indent)
// FIXME: dedup with adsc.h
#define CAST_PTR(x, t, y) t x = (t)(y)
#define IFPRINTF(s, i, f, ...) fprintf(s, "%*s" f, i, "", __VA_ARGS__)

// asn1-format.c
void output_asn1(FILE *stream, asn_TYPE_descriptor_t *td, const void *sptr, int indent);
