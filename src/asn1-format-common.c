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
#include "asn1-util.h"              // ASN1_FORMATTER_PROTOTYPE
#include "dumpvdl2.h"               // CAST_PTR, dict_search()

char const *value2enum(asn_TYPE_descriptor_t *td, long const value) {
	if(td == NULL) return NULL;
	asn_INTEGER_enum_map_t const *enum_map = INTEGER_map_value2enum(td->specifics, value);
	if(enum_map == NULL) return NULL;
	return enum_map->enum_name;
}

void _format_INTEGER_with_unit_as_text(la_vstring *vstr, char const * const label, asn_TYPE_descriptor_t *td,
		void const *sptr, int indent, char const * const unit, double multiplier, int decimal_places) {
	UNUSED(td);
	CAST_PTR(val, long *, sptr);
	LA_ISPRINTF(vstr, indent, "%s: %.*f%s\n", label, decimal_places, (double)(*val) * multiplier, unit);
}

void _format_INTEGER_with_unit_as_json(la_vstring *vstr, char const * const label, asn_TYPE_descriptor_t *td,
		void const *sptr, int indent, char const * const unit, double multiplier, int decimal_places) {
	UNUSED(td);
	UNUSED(indent);
	UNUSED(decimal_places);
	CAST_PTR(val, long *, sptr);
	la_json_object_start(vstr, label);
	la_json_append_double(vstr, "val", (double)(*val) * multiplier);
	la_json_append_string(vstr, "unit", unit);
	la_json_object_end(vstr);
}

void _format_INTEGER_as_ENUM_as_text(la_vstring *vstr, char const * const label, dict const * const value_labels,
		void const *sptr, int indent) {
	CAST_PTR(val, long *, sptr);
	char *val_label = dict_search(value_labels, (int)(*val));
	if(val_label != NULL) {
		LA_ISPRINTF(vstr, indent, "%s: %s\n", label, val_label);
	} else {
		LA_ISPRINTF(vstr, indent, "%s: %ld (unknown)\n", label, *val);
	}
}

void _format_INTEGER_as_ENUM_as_json(la_vstring *vstr, char const * const label, dict const * const value_labels,
		void const *sptr, int indent) {
	UNUSED(indent);
	CAST_PTR(val, long *, sptr);
	la_json_object_start(vstr, label);
	la_json_append_long(vstr, "value", (int)(*val));
	char *val_label = dict_search(value_labels, (int)(*val));
	JSON_APPEND_STRING(vstr, "value_descr", val_label);
	la_json_object_end(vstr);
}

void _format_CHOICE_as_text(la_vstring *vstr, char const * const label, dict const * const choice_labels,
		asn1_output_fun_t cb, asn_TYPE_descriptor_t *td, void const *sptr, int indent) {

	CAST_PTR(specs, asn_CHOICE_specifics_t *, td->specifics);
	int present = _fetch_present_idx(sptr, specs->pres_offset, specs->pres_size);
	if(label != NULL) {
		LA_ISPRINTF(vstr, indent, "%s:\n", label);
		indent++;
	}
	if(choice_labels != NULL) {
		char *descr = dict_search(choice_labels, present);
		if(descr != NULL) {
			LA_ISPRINTF(vstr, indent, "%s\n", descr);
		} else {
			LA_ISPRINTF(vstr, indent, "<no description for CHOICE value %d>\n", present);
		}
		indent++;
	}
	if(present > 0 && present <= td->elements_count) {
		asn_TYPE_member_t *elm = &td->elements[present-1];
		void const *memb_ptr;

		if(elm->flags & ATF_POINTER) {
			memb_ptr = *(const void * const *)((const char *)sptr + elm->memb_offset);
			if(!memb_ptr) {
				LA_ISPRINTF(vstr, indent, "%s: <not present>\n", elm->name);
				return;
			}
		} else {
			memb_ptr = (const void *)((const char *)sptr + elm->memb_offset);
		}

		cb(vstr, elm->type, memb_ptr, indent);
	} else {
		LA_ISPRINTF(vstr, indent, "-- %s: value %d out of range\n", td->name, present);
	}
}

void _format_CHOICE_as_json(la_vstring *vstr, char const * const label, dict const * const choice_labels,
		asn1_output_fun_t cb, asn_TYPE_descriptor_t *td, void const *sptr, int indent) {
	UNUSED(indent);
	asn_CHOICE_specifics_t *specs = (asn_CHOICE_specifics_t *)td->specifics;
	int present = _fetch_present_idx(sptr, specs->pres_offset, specs->pres_size);
	la_json_object_start(vstr, label);
	if(choice_labels != NULL) {
		char *descr = dict_search(choice_labels, present);
		la_json_append_string(vstr, "choice_label", descr != NULL ? descr : "");
	}
	if(present > 0 && present <= td->elements_count) {
		asn_TYPE_member_t *elm = &td->elements[present-1];
		void const *memb_ptr;

		if(elm->flags & ATF_POINTER) {
			memb_ptr = *(const void * const *)((const char *)sptr + elm->memb_offset);
			if(!memb_ptr) {
				goto end;
			}
		} else {
			memb_ptr = (const void *)((const char *)sptr + elm->memb_offset);
		}
		la_json_append_string(vstr, "choice", elm->name);
		la_json_object_start(vstr, "data");
		cb(vstr, elm->type, memb_ptr, 0);
		la_json_object_end(vstr);
	}
end:
	la_json_object_end(vstr);
}

void _format_SEQUENCE_as_text(la_vstring *vstr, char const * const label, asn1_output_fun_t cb,
		asn_TYPE_descriptor_t *td, void const *sptr, int indent) {
	if(label != NULL) {
		LA_ISPRINTF(vstr, indent, "%s:\n", label);
		indent++;
	}
	for(int edx = 0; edx < td->elements_count; edx++) {
		asn_TYPE_member_t *elm = &td->elements[edx];
		const void *memb_ptr;

		if(elm->flags & ATF_POINTER) {
			memb_ptr = *(const void * const *)((const char *)sptr + elm->memb_offset);
			if(!memb_ptr) {
				continue;
			}
		} else {
			memb_ptr = (const void *)((const char *)sptr + elm->memb_offset);
		}
		cb(vstr, elm->type, memb_ptr, indent);
	}
}

void _format_SEQUENCE_as_json(la_vstring *vstr, char const * const label, asn1_output_fun_t cb,
		asn_TYPE_descriptor_t *td, void const *sptr, int indent) {
	UNUSED(indent);
	la_json_array_start(vstr, label);
	for(int edx = 0; edx < td->elements_count; edx++) {
		asn_TYPE_member_t *elm = &td->elements[edx];
		const void *memb_ptr;

		if(elm->flags & ATF_POINTER) {
			memb_ptr = *(const void * const *)((const char *)sptr + elm->memb_offset);
			if(!memb_ptr) {
				continue;
			}
		} else {
			memb_ptr = (const void *)((const char *)sptr + elm->memb_offset);
		}
		la_json_object_start(vstr, NULL);
		cb(vstr, elm->type, memb_ptr, 0);
		la_json_object_end(vstr);
	}
	la_json_array_end(vstr);
}

void _format_SEQUENCE_OF_as_text(la_vstring *vstr, char const * const label, asn1_output_fun_t cb,
		asn_TYPE_descriptor_t *td, void const *sptr, int indent) {
	if(label != NULL) {
		LA_ISPRINTF(vstr, indent, "%s:\n", label);
		indent++;
	}
	asn_TYPE_member_t *elm = td->elements;
	const asn_anonymous_set_ *list = _A_CSET_FROM_VOID(sptr);
	for(int i = 0; i < list->count; i++) {
		const void *memb_ptr = list->array[i];
		if(memb_ptr == NULL) {
			continue;
		}
		cb(vstr, elm->type, memb_ptr, indent);
	}
}

void _format_SEQUENCE_OF_as_json(la_vstring *vstr, char const * const label, asn1_output_fun_t cb,
		asn_TYPE_descriptor_t *td, void const *sptr, int indent) {
	UNUSED(indent);
	la_json_array_start(vstr, label);
	asn_TYPE_member_t *elm = td->elements;
	const asn_anonymous_set_ *list = _A_CSET_FROM_VOID(sptr);
	for(int i = 0; i < list->count; i++) {
		const void *memb_ptr = list->array[i];
		if(memb_ptr == NULL) {
			continue;
		}
		la_json_object_start(vstr, NULL);
		cb(vstr, elm->type, memb_ptr, 0);
		la_json_object_end(vstr);
	}
	la_json_array_end(vstr);
}

// Handles bit string up to 32 bits long.
// dict indices are bit numbers from 0 to bit_stream_len-1
// Bit 0 is the MSB of the first octet in the buffer.
void _format_BIT_STRING_as_text(la_vstring *vstr, char const * const label, dict const * const bit_labels,
		void const *sptr, int indent) {
	CAST_PTR(bs, BIT_STRING_t *, sptr);
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
	if(label != NULL) {
		LA_ISPRINTF(vstr, indent, "%s: ", label);
	}
	for(int i = 0; i < len; val = (val << 8) | bs->buf[i++])
		;
	debug_print(D_PROTO_DETAIL, "val: 0x%08x\n", val);
	val &= (~0u << bits_unused);    // zeroize unused bits
	if(val == 0) {
		la_vstring_append_sprintf(vstr, "none\n");
		goto end;
	}
	val = reverse(val, len * 8);
	bool first = true;
	for(dict const *ptr = bit_labels; ptr->val != NULL; ptr++) {
		uint32_t shift = (uint32_t)ptr->id;
		if((val >> shift) & 1) {
			la_vstring_append_sprintf(vstr, "%s%s",
					(first ? "" : ", "), (char *)ptr->val);
			first = false;
		}
	}
	EOL(vstr);
end:
	if(truncated > 0) {
		LA_ISPRINTF(vstr, indent,
				"-- Warning: bit string too long (%d bits), truncated to %d bits\n",
				bs->size * 8 - bs->bits_unused, len * 8);
	}
}

void _format_BIT_STRING_as_json(la_vstring *vstr, char const * const label, dict const * const bit_labels,
		void const *sptr, int indent) {
	UNUSED(indent);
	CAST_PTR(bs, BIT_STRING_t *, sptr);
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
	la_json_array_start(vstr, label);
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
			la_json_append_string(vstr, NULL, (char *)ptr->val);
		}
	}
end:
	la_json_array_end(vstr);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_any_as_text) {
	if(label != NULL) {
		LA_ISPRINTF(vstr, indent, "%s: ", label);
	} else {
		LA_ISPRINTF(vstr, indent, "%s", "");
	}
	asn_sprintf(vstr, td, sptr, 1);
	EOL(vstr);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_any_as_string_as_json) {
	UNUSED(indent);
	la_vstring *tmp = la_vstring_new();
	asn_sprintf(tmp, td, sptr, 0);
	la_json_append_string(vstr, label, tmp->str);
	la_vstring_destroy(tmp, true);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_NULL) {
	UNUSED(td);
	UNUSED(label);
	UNUSED(vstr);
	UNUSED(sptr);
	UNUSED(indent);
	// NOOP
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_label_only_as_text) {
	UNUSED(td);
	UNUSED(sptr);
	if(label != NULL) {
		LA_ISPRINTF(vstr, indent, "%s\n", label);
	}
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_label_only_as_json) {
	UNUSED(td);
	UNUSED(sptr);
	UNUSED(indent);
	if(label != NULL) {
		la_json_object_start(vstr, label);
		la_json_object_end(vstr);
	}
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_ENUM_as_text) {
	long const value = *(long const *)sptr;
	char const *s = value2enum(td, value);
	if(s != NULL) {
		LA_ISPRINTF(vstr, indent, "%s: %s\n", label, s);
	} else {
		LA_ISPRINTF(vstr, indent, "%s: %ld\n", label, value);
	}
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_ENUM_as_json) {
	UNUSED(indent);
	long const value = *(long const *)sptr;
	char const *s = value2enum(td, value);
	if(s != NULL) {
		la_json_append_string(vstr, label, s);
	} else {
		la_json_append_long(vstr, label, value);
	}
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_Deg_as_text) {
	_format_INTEGER_with_unit_as_text(vstr, label, td, sptr, indent, " deg", 1, 0);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_Deg_as_json) {
	_format_INTEGER_with_unit_as_json(vstr, label, td, sptr, indent, "deg", 1, 0);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_long_as_json) {
	UNUSED(td);
	UNUSED(indent);

	CAST_PTR(valptr, long *, sptr);
	la_json_append_long(vstr, label, *valptr);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_bool_as_json) {
	UNUSED(td);
	UNUSED(indent);

	CAST_PTR(valptr, BOOLEAN_t *, sptr);
	la_json_append_bool(vstr, label, (*valptr) ? true : false);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_OCTET_STRING_as_json) {
	UNUSED(td);
	UNUSED(indent);

	CAST_PTR(valptr, OCTET_STRING_t *, sptr);
	la_json_append_octet_string(vstr, label, valptr->buf, valptr->size);
}
