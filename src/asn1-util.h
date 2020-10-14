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

#ifndef _ASN1_UTIL_H
#define _ASN1_UTIL_H
#include <stdint.h>                 // uint8_t
#include <libacars/libacars.h>      // la_type_descriptor
#include "asn1/constr_TYPE.h"       // asn_TYPE_descriptor_t

// Parameters to the formatter function
typedef struct {
	la_vstring *vstr;
	char const * const label;
	asn_TYPE_descriptor_t *td;
	void const *sptr;
	int indent;
} asn1_formatter_param_t;

// Formatter function prototype
typedef void (*asn1_formatter_fun_t)(asn1_formatter_param_t);

typedef struct {
	asn_TYPE_descriptor_t *type;
	asn1_formatter_fun_t format;
	char const * const label;
} asn_formatter_t;

// A structure for storing decoded ASN.1 payloads in a la_proto_node
typedef struct {
	asn_TYPE_descriptor_t *type;
	void *data;
	asn_formatter_t const *formatter_table_text;
	asn_formatter_t const *formatter_table_json;
	size_t formatter_table_text_len;
	size_t formatter_table_json_len;
} asn1_pdu_t;

#define ASN1_FORMATTER_PROTOTYPE(x) \
	void x(asn1_formatter_param_t p)

// asn1-util.c
int asn1_decode_as(asn_TYPE_descriptor_t *td, void **struct_ptr, uint8_t *buf, int size);
void asn1_output_as_text(la_vstring *vstr, asn_formatter_t const * const asn1_formatter_table,
		size_t asn1_formatter_table_len, asn_TYPE_descriptor_t *td, const void *sptr, int indent);
void asn1_output_as_json(la_vstring *vstr, asn_formatter_t const * const asn1_formatter_table,
		size_t asn1_formatter_table_len, asn_TYPE_descriptor_t *td, const void *sptr);
void asn1_pdu_format_text(la_vstring *vstr, void const * const data, int indent);
void asn1_pdu_format_json(la_vstring *vstr, void const * const data);
void asn1_pdu_destroy(void *data);

#endif // _ASN1_UTIL_H
