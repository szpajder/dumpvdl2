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
#include <stdio.h>
#include <stdlib.h>
#include <libacars/vstring.h>		// la_vstring
#include "tlv.h"
#include "dumpvdl2.h"

void tlv_list_free(tlv_list_t *p) {
	if(p != NULL)
		tlv_list_free(p->next);
	XFREE(p);
}

void tlv_list_append(tlv_list_t **head, uint8_t type, uint16_t len, uint8_t *value) {
	tlv_list_t *ptr;
	if(*head == NULL) {	// new list
		*head = ptr = XCALLOC(1, sizeof(tlv_list_t));
	} else {		// existing list
		for(ptr = *head; ptr->next != NULL; ptr = ptr->next)
			;
		ptr->next = XCALLOC(1, sizeof(tlv_list_t));
		ptr = ptr->next;
	}
	ptr->type = type;
	ptr->len = len;
	ptr->val = value;
}

tlv_list_t *tlv_list_search(tlv_list_t *ptr, uint8_t type) {
	while(ptr != NULL) {
		if(ptr->type == type) break;
		ptr = ptr->next;
	}
	return ptr;
}

tlv_list_t *tlv_deserialize(uint8_t *buf, uint16_t len, uint8_t len_octets) {
	tlv_list_t *head = NULL;
	uint8_t *ptr = buf;
	uint8_t tlv_min_paramlen = 1 + len_octets;	/* code + <len_octets> length field + empty data field */
	uint16_t paramlen;
	while(len >= tlv_min_paramlen) {
		uint8_t pid = *ptr;
		ptr++; len--;

		paramlen = *ptr;
		if(len_octets == 2)
			paramlen = (paramlen << 8) | (uint16_t)ptr[1];

		ptr += len_octets; len -= len_octets;
		if(paramlen > len) {
			debug_print("TLV param %02x truncated: paramlen=%u buflen=%u\n", pid, paramlen, len);
			return NULL;
		}
		tlv_list_append(&head, pid, paramlen, ptr);
		ptr += paramlen; len -= paramlen;
	}
	if(len > 0)
		debug_print("Warning: %u unparsed octets left at end of TLV list\n", len);
	return head;
}

tlv_dict *tlv_dict_search(const tlv_dict *list, uint8_t id) {
	if(list == NULL) return NULL;
	tlv_dict *ptr;
	for(ptr = (tlv_dict *)list; ; ptr++) {
		if(ptr->description == NULL) return NULL;
		if(ptr->id == id) return ptr;
	}
}

void tlv_format_as_text(la_vstring *vstr, tlv_list_t *list, const tlv_dict *d, int indent) {
	if(list == NULL || d == NULL) return;
	ASSERT(vstr != NULL);
	ASSERT(indent >= 0);
	for(tlv_list_t *p = list; p != NULL; p = p->next) {
		tlv_dict *entry = tlv_dict_search(d, p->type);
		char *str = NULL;
		if(entry != NULL) {
			str = (*(entry->stringify))(p->val, p->len);
			LA_ISPRINTF(vstr, indent, "%s: %s\n", entry->description, str);
		} else {
			str = fmt_hexstring(p->val, p->len);
			LA_ISPRINTF(vstr, indent, "(Unknown code 0x%02x): %s\n", p->type, str);
		}
		XFREE(str);
	}
}
