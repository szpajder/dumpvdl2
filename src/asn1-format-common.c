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

#include "asn1/asn_application.h"   // asn_TYPE_descriptor_t, asn_sprintf
#include "asn1/INTEGER.h"           // asn_INTEGER_enum_map_t
#include "asn1/constr_CHOICE.h"     // _fetch_present_idx()
#include "asn1/asn_SET_OF.h"        // _A_CSET_FROM_VOID()
#include "asn1/BIT_STRING.h"        // BIT_STRING_t;
#include "asn1/OCTET_STRING.h"      // OCTET_STRING_t;
#include "asn1/BOOLEAN.h"           // BOOLEAN_t
#include <libacars/vstring.h>       // la_vstring, LA_ISPRINTF()
#include <libacars/json.h>
#include "asn1-util.h"              // ASN1_FORMATTER_FUN_T
#include "dumpvdl2.h"               // dict_search()

char const *value2enum(asn_TYPE_descriptor_t *td, long value) {
	if(td == NULL) return NULL;
	asn_INTEGER_enum_map_t const *enum_map = INTEGER_map_value2enum(td->specifics, value);
	if(enum_map == NULL) return NULL;
	return enum_map->enum_name;
}

void format_INTEGER_with_unit_as_text(asn1_formatter_param_t p,
		char const *unit, double multiplier, int decimal_places) {
	long const *val = p.sptr;
	LA_ISPRINTF(p.vstr, p.indent, "%s: %.*f%s\n", p.label, decimal_places, (double)(*val) * multiplier, unit);
}

void format_INTEGER_with_unit_as_json(asn1_formatter_param_t p,
		char const *unit, double multiplier) {
	long const *val = p.sptr;
	la_json_object_start(p.vstr, p.label);
	la_json_append_double(p.vstr, "val", (double)(*val) * multiplier);
	la_json_append_string(p.vstr, "unit", unit);
	la_json_object_end(p.vstr);
}

void format_INTEGER_as_ENUM_as_text(asn1_formatter_param_t p, dict const *value_labels) {
	long const *val = p.sptr;
	char const *val_label = dict_search(value_labels, (int)(*val));
	if(val_label != NULL) {
		LA_ISPRINTF(p.vstr, p.indent, "%s: %s\n", p.label, val_label);
	} else {
		LA_ISPRINTF(p.vstr, p.indent, "%s: %ld (unknown)\n", p.label, *val);
	}
}

void format_INTEGER_as_ENUM_as_json(asn1_formatter_param_t p, dict const *value_labels) {
	long const *val = p.sptr;
	la_json_object_start(p.vstr, p.label);
	la_json_append_long(p.vstr, "value", (int)(*val));
	char const *val_label = dict_search(value_labels, (int)(*val));
	SAFE_JSON_APPEND_STRING(p.vstr, "value_descr", val_label);
	la_json_object_end(p.vstr);
}

void format_CHOICE_as_text(asn1_formatter_param_t p, dict const *choice_labels,
		asn1_formatter_fun_t cb) {
	asn_CHOICE_specifics_t *specs = p.td->specifics;
	int present = _fetch_present_idx(p.sptr, specs->pres_offset, specs->pres_size);
	if(p.label != NULL) {
		LA_ISPRINTF(p.vstr, p.indent, "%s:\n", p.label);
		p.indent++;
	}
	if(choice_labels != NULL) {
		char const *descr = dict_search(choice_labels, present);
		if(descr != NULL) {
			LA_ISPRINTF(p.vstr, p.indent, "%s\n", descr);
		} else {
			LA_ISPRINTF(p.vstr, p.indent, "<no description for CHOICE value %d>\n", present);
		}
		p.indent++;
	}
	if(present > 0 && present <= p.td->elements_count) {
		asn_TYPE_member_t *elm = &p.td->elements[present-1];
		void const *memb_ptr;

		if(elm->flags & ATF_POINTER) {
			memb_ptr = *(void const * const *)((char const *)p.sptr + elm->memb_offset);
			if(!memb_ptr) {
				LA_ISPRINTF(p.vstr, p.indent, "%s: <not present>\n", elm->name);
				return;
			}
		} else {
			memb_ptr = (void const *)((char const *)p.sptr + elm->memb_offset);
		}

		p.td = elm->type;
		p.sptr = memb_ptr;
		cb(p);
	} else {
		LA_ISPRINTF(p.vstr, p.indent, "-- %s: value %d out of range\n", p.td->name, present);
	}
}

void format_CHOICE_as_json(asn1_formatter_param_t p, dict const *choice_labels,
		asn1_formatter_fun_t cb) {
	asn_CHOICE_specifics_t const *specs = p.td->specifics;
	int present = _fetch_present_idx(p.sptr, specs->pres_offset, specs->pres_size);
	la_json_object_start(p.vstr, p.label);
	if(choice_labels != NULL) {
		char const *descr = dict_search(choice_labels, present);
		la_json_append_string(p.vstr, "choice_label", descr != NULL ? descr : "");
	}
	if(present > 0 && present <= p.td->elements_count) {
		asn_TYPE_member_t *elm = &p.td->elements[present-1];
		void const *memb_ptr;

		if(elm->flags & ATF_POINTER) {
			memb_ptr = *(void const * const *)((char const *)p.sptr + elm->memb_offset);
			if(!memb_ptr) {
				goto end;
			}
		} else {
			memb_ptr = (void const *)((char const *)p.sptr + elm->memb_offset);
		}
		la_json_append_string(p.vstr, "choice", elm->name);
		la_json_object_start(p.vstr, "data");
		p.td = elm->type;
		p.sptr = memb_ptr;
		cb(p);
		la_json_object_end(p.vstr);
	}
end:
	la_json_object_end(p.vstr);
}

void format_SEQUENCE_as_text(asn1_formatter_param_t p, asn1_formatter_fun_t cb) {
	if(p.label != NULL) {
		LA_ISPRINTF(p.vstr, p.indent, "%s:\n", p.label);
		p.indent++;
	}
	asn1_formatter_param_t cb_p = p;
	for(int edx = 0; edx < p.td->elements_count; edx++) {
		asn_TYPE_member_t *elm = &p.td->elements[edx];
		void const *memb_ptr;

		if(elm->flags & ATF_POINTER) {
			memb_ptr = *(void const * const *)((char const *)p.sptr + elm->memb_offset);
			if(!memb_ptr) {
				continue;
			}
		} else {
			memb_ptr = (void const *)((char const *)p.sptr + elm->memb_offset);
		}
		cb_p.td = elm->type;
		cb_p.sptr = memb_ptr;
		cb(cb_p);
	}
}

// Prints ASN.1 SEQUENCE as JSON object.
// All fields in the sequence must have unique types (and p.labels), otherwise
// JSON keys will clash.
void format_SEQUENCE_as_json(asn1_formatter_param_t p, asn1_formatter_fun_t cb) {
	asn1_formatter_param_t cb_p = p;
	la_json_object_start(p.vstr, p.label);
	for(int edx = 0; edx < p.td->elements_count; edx++) {
		asn_TYPE_member_t *elm = &p.td->elements[edx];
		void const *memb_ptr;

		if(elm->flags & ATF_POINTER) {
			memb_ptr = *(void const * const *)((char const *)p.sptr + elm->memb_offset);
			if(!memb_ptr) {
				continue;
			}
		} else {
			memb_ptr = (void const *)((char const *)p.sptr + elm->memb_offset);
		}
		cb_p.td = elm->type;
		cb_p.sptr = memb_ptr;
		cb(cb_p);
	}
	la_json_object_end(p.vstr);
}

void format_SEQUENCE_OF_as_text(asn1_formatter_param_t p, asn1_formatter_fun_t cb) {
	if(p.label != NULL) {
		LA_ISPRINTF(p.vstr, p.indent, "%s:\n", p.label);
		p.indent++;
	}
	asn_TYPE_member_t *elm = p.td->elements;
	asn_anonymous_set_ const *list = _A_CSET_FROM_VOID(p.sptr);
	for(int i = 0; i < list->count; i++) {
		void const *memb_ptr = list->array[i];
		if(memb_ptr == NULL) {
			continue;
		}
		p.td = elm->type;
		p.sptr = memb_ptr;
		cb(p);
	}
}

void format_SEQUENCE_OF_as_json(asn1_formatter_param_t p, asn1_formatter_fun_t cb) {
	la_json_array_start(p.vstr, p.label);
	asn_TYPE_member_t *elm = p.td->elements;
	asn_anonymous_set_ const *list = _A_CSET_FROM_VOID(p.sptr);
	for(int i = 0; i < list->count; i++) {
		void const *memb_ptr = list->array[i];
		if(memb_ptr == NULL) {
			continue;
		}
		la_json_object_start(p.vstr, NULL);
		p.td = elm->type;
		p.sptr = memb_ptr;
		cb(p);
		la_json_object_end(p.vstr);
	}
	la_json_array_end(p.vstr);
}

// Handles bit string up to 32 bits long.
// dict indices are bit numbers from 0 to bit_stream_len-1
// Bit 0 is the MSB of the first octet in the buffer.
void format_BIT_STRING_as_text(asn1_formatter_param_t p, dict const *bit_labels) {
	BIT_STRING_t const *bs = p.sptr;
	debug_print(D_PROTO_DETAIL, "buf len: %d bits_unused: %d\n", bs->size, bs->bits_unused);
	uint32_t val = 0;
	int truncated = 0;
	int len = bs->size;
	int bits_unused = bs->bits_unused;

	if(len > (int)sizeof(val)) {
		debug_print(D_PROTO, "bit stream too long (%d octets), truncating to %zu octets\n",
				len, sizeof(val));
		truncated = len - sizeof(val);
		len = sizeof(val);
		bits_unused = 0;
	}
	if(p.label != NULL) {
		LA_ISPRINTF(p.vstr, p.indent, "%s: ", p.label);
	}
	for(int i = 0; i < len; val = (val << 8) | bs->buf[i++])
		;
	debug_print(D_PROTO_DETAIL, "val: 0x%08x\n", val);
	val &= (~0u << bits_unused);    // zeroize unused bits
	if(val == 0) {
		la_vstring_append_sprintf(p.vstr, "none\n");
		goto end;
	}
	val = reverse(val, len * 8);
	bool first = true;
	for(dict const *ptr = bit_labels; ptr->val != NULL; ptr++) {
		uint32_t shift = (uint32_t)ptr->id;
		if((val >> shift) & 1) {
			la_vstring_append_sprintf(p.vstr, "%s%s",
					(first ? "" : ", "), (char *)ptr->val);
			first = false;
		}
	}
	EOL(p.vstr);
end:
	if(truncated > 0) {
		LA_ISPRINTF(p.vstr, p.indent,
				"-- Warning: bit string too long (%d bits), truncated to %d bits\n",
				bs->size * 8 - bs->bits_unused, len * 8);
	}
}

void format_BIT_STRING_as_json(asn1_formatter_param_t p, dict const *bit_labels) {
	BIT_STRING_t const *bs = p.sptr;
	debug_print(D_PROTO_DETAIL, "buf len: %d bits_unused: %d\n", bs->size, bs->bits_unused);
	uint32_t val = 0;
	int len = bs->size;
	int bits_unused = bs->bits_unused;

	if(len > (int)sizeof(val)) {
		debug_print(D_PROTO, "bit stream too long (%d octets), truncating to %zu octets\n",
				len, sizeof(val));
		len = sizeof(val);
		bits_unused = 0;
	}
	la_json_array_start(p.vstr, p.label);
	for(int i = 0; i < len; val = (val << 8) | bs->buf[i++])
		;
	debug_print(D_PROTO_DETAIL, "val: 0x%08x\n", val);
	val &= (~0u << bits_unused);    // zeroize unused bits
	if(val == 0) {
		goto end;
	}
	val = reverse(val, len * 8);
	for(dict const *ptr = bit_labels; ptr->val != NULL; ptr++) {
		uint32_t shift = (uint32_t)ptr->id;
		if((val >> shift) & 1) {
			la_json_append_string(p.vstr, NULL, (char *)ptr->val);
		}
	}
end:
	la_json_array_end(p.vstr);
}

ASN1_FORMATTER_FUN_T(asn1_format_any_as_text) {
	if(p.label != NULL) {
		LA_ISPRINTF(p.vstr, p.indent, "%s: ", p.label);
	} else {
		LA_ISPRINTF(p.vstr, p.indent, "%s", "");
	}
	asn_sprintf(p.vstr, p.td, p.sptr, 1);
	EOL(p.vstr);
}

ASN1_FORMATTER_FUN_T(asn1_format_any_as_string_as_json) {
	UNUSED(p.indent);
	la_vstring *tmp = la_vstring_new();
	asn_sprintf(tmp, p.td, p.sptr, 0);
	la_json_append_string(p.vstr, p.label, tmp->str);
	la_vstring_destroy(tmp, true);
}

ASN1_FORMATTER_FUN_T(asn1_format_label_only_as_text) {
	if(p.label != NULL) {
		LA_ISPRINTF(p.vstr, p.indent, "%s\n", p.label);
	}
}

ASN1_FORMATTER_FUN_T(asn1_format_label_only_as_json) {
	if(p.label != NULL) {
		la_json_object_start(p.vstr, p.label);
		la_json_object_end(p.vstr);
	}
}

ASN1_FORMATTER_FUN_T(asn1_format_ENUM_as_text) {
	long const value = *(long const *)p.sptr;
	char const *s = value2enum(p.td, value);
	if(s != NULL) {
		LA_ISPRINTF(p.vstr, p.indent, "%s: %s\n", p.label, s);
	} else {
		LA_ISPRINTF(p.vstr, p.indent, "%s: %ld\n", p.label, value);
	}
}

ASN1_FORMATTER_FUN_T(asn1_format_ENUM_as_json) {
	long const value = *(long const *)p.sptr;
	char const *s = value2enum(p.td, value);
	if(s != NULL) {
		la_json_append_string(p.vstr, p.label, s);
	} else {
		la_json_append_long(p.vstr, p.label, value);
	}
}

ASN1_FORMATTER_FUN_T(asn1_format_long_as_json) {
	long const *valptr = p.sptr;
	la_json_append_long(p.vstr, p.label, *valptr);
}

ASN1_FORMATTER_FUN_T(asn1_format_bool_as_json) {
	BOOLEAN_t const *valptr = p.sptr;
	la_json_append_bool(p.vstr, p.label, (*valptr) ? true : false);
}

ASN1_FORMATTER_FUN_T(asn1_format_OCTET_STRING_as_json) {
	OCTET_STRING_t const *valptr = p.sptr;
	la_json_append_octet_string(p.vstr, p.label, valptr->buf, valptr->size);
}
