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
#ifndef _TLV2_H
#define _TLV2_H 1
#include <stdio.h>
#include <stdint.h>
#include <libacars/list.h>		// la_list
#include <libacars/vstring.h>		// la_vstring

typedef struct {
	la_vstring *vstr;
	int indent;
} tlv2_formatter_ctx_t;

typedef void *(tlv2_parser_f)(uint8_t typecode, uint8_t *buf, size_t len);
typedef void(tlv2_formatter_f)(tlv2_formatter_ctx_t * const ctx, char const * const label, void const * const data);
typedef void(tlv2_destructor_f)(void *data);

#define TLV2_PARSER(x) void *(x)(uint8_t typecode, uint8_t *buf, size_t len)
#define TLV2_FORMATTER(x) void (x)(tlv2_formatter_ctx_t * const ctx, char const * const label, void const * const data)
#define TLV2_DESTRUCTOR(x) void (x)(void *data)

typedef struct {
	char const * const label;
	char const * const json_key;
	tlv2_parser_f *parse;
	tlv2_formatter_f *format_text;
	tlv2_formatter_f *format_json;
	tlv2_destructor_f *destroy;
} tlv2_type_descriptor_t;

// generic tag structure
typedef struct {
	uint8_t typecode;
	tlv2_type_descriptor_t *td;
	void *data;
} tlv2_tag_t;

// tlv2.c
// Generic TLV API
la_list *tlv2_parse(uint8_t *buf, size_t len, dict const *tag_table, size_t const len_octets);
la_list *tlv2_single_tag_parse(uint8_t typecode, uint8_t *buf, size_t tag_len, dict const *tag_table, la_list *list);
tlv2_tag_t *tlv2_list_search(la_list *ptr, uint8_t const typecode);
la_list *tlv2_list_append(la_list *head, uint8_t typecode, tlv2_type_descriptor_t *td, uint8_t *data);
void tlv2_list_format_text(la_vstring * const vstr, la_list *tlv_list, int indent);
void tlv2_list_destroy(la_list *p);

// Parsers and formatters for common data types
TLV2_PARSER(tlv2_octet_string_parse);
TLV2_FORMATTER(tlv2_octet_string_format_text);

TLV2_PARSER(tlv2_uint8_parse);
TLV2_PARSER(tlv2_uint16_msbfirst_parse);
TLV2_PARSER(tlv2_uint32_msbfirst_parse);
TLV2_FORMATTER(tlv2_uint_format_text);
TLV2_FORMATTER(tlv2_octet_string_with_ascii_format_text);
TLV2_DESTRUCTOR(tlv2_destroy_noop);
#endif // !_TLV2_H
