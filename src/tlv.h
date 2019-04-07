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
#ifndef _TLV_H
#define _TLV_H 1
#include <stdio.h>
#include <stdint.h>
#include <libacars/vstring.h>		// la_vstring

typedef struct {
	void *next;
	uint8_t *val;
	uint16_t len;
	uint8_t type;
} tlv_list_t;

typedef struct {
	uint8_t id;
	void *val;
} dict;

typedef struct {
	uint8_t id;
	char *(*stringify)(uint8_t *, uint16_t);
	char *description;
} tlv_dict;

// tlv.c
void tlv_list_free(tlv_list_t *p);
void tlv_list_append(tlv_list_t **head, uint8_t type, uint16_t len, uint8_t *value);
void output_tlv(FILE *f, tlv_list_t *list, const tlv_dict *d);
void tlv_format_as_text(la_vstring *vstr, tlv_list_t *list, const tlv_dict *d, int indent);
void *dict_search(const dict *list, uint8_t id);
tlv_list_t *tlv_list_search(tlv_list_t *ptr, uint8_t type);
tlv_list_t *tlv_deserialize(uint8_t *buf, uint16_t len, uint8_t len_octets);
#endif // !_TLV_H
