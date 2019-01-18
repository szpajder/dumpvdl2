/*
 *  This file is a part of dumpvdl2
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

#ifndef _ASN1_UTIL_H
#define _ASN1_UTIL_H
#include <stdint.h>			// uint8_t
#include "asn1/constr_TYPE.h"		// asn_TYPE_descriptor_t

typedef struct {
	asn_TYPE_descriptor_t *type;
	void (*format)(FILE *, char const * const label, asn_TYPE_descriptor_t *, const void *, int);
	char const * const label;
} asn_formatter_t;
typedef void (*asn1_output_fun_t)(FILE *, asn_TYPE_descriptor_t *, const void *, int);

#define ASN1_FORMATTER_PROTOTYPE(x) void x(FILE *stream, char const * const label, asn_TYPE_descriptor_t *td, void const *sptr, int indent)
#define CAST_PTR(x, t, y) t x = (t)(y)
#define IFPRINTF(s, i, f, ...) fprintf(s, "%*s" f, i, "", __VA_ARGS__)

// asn1-util.c
int asn1_decode_as(asn_TYPE_descriptor_t *td, void **struct_ptr, uint8_t *buf, int size);
void asn1_output(FILE *stream, asn_formatter_t const * const asn1_formatter_table,
	size_t asn1_formatter_table_len, asn_TYPE_descriptor_t *td, const void *sptr, int indent);

#endif // _ASN1_UTIL_H
