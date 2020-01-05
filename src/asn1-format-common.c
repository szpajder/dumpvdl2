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

#include "asn1/asn_application.h"	// asn_TYPE_descriptor_t, asn_sprintf
#include "asn1/INTEGER.h"		// asn_INTEGER_enum_map_t
#include "asn1/constr_CHOICE.h"		// _fetch_present_idx()
#include "asn1/asn_SET_OF.h"		// _A_CSET_FROM_VOID()
#include "asn1/BIT_STRING.h"		// BIT_STRING_t;
#include <libacars/vstring.h>		// la_vstring, LA_ISPRINTF()
#include "asn1-util.h"			// ASN1_FORMATTER_PROTOTYPE
#include "dumpvdl2.h"			// CAST_PTR, dict_search()

char const *value2enum(asn_TYPE_descriptor_t *td, long const value) {
	if(td == NULL) return NULL;
	asn_INTEGER_enum_map_t const *enum_map = INTEGER_map_value2enum(td->specifics, value);
	if(enum_map == NULL) return NULL;
	return enum_map->enum_name;
}

void _format_INTEGER_with_unit(la_vstring *vstr, char const * const label, asn_TYPE_descriptor_t *td,
	void const *sptr, int indent, char const * const unit, double multiplier, int decimal_places) {
	UNUSED(td);
	CAST_PTR(val, long *, sptr);
	LA_ISPRINTF(vstr, indent, "%s: %.*f%s\n", label, decimal_places, (double)(*val) * multiplier, unit);
}

void _format_CHOICE(la_vstring *vstr, char const * const label, dict const * const choice_labels,
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

void _format_SEQUENCE(la_vstring *vstr, char const * const label, asn1_output_fun_t cb,
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

void _format_SEQUENCE_OF(la_vstring *vstr, char const * const label, asn1_output_fun_t cb,
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

// Handles bit string up to 32 bits long.
// dict indices are bit numbers from 0 to bit_stream_len-1
// Bit 0 is the MSB of the first octet in the buffer.
void _format_BIT_STRING(la_vstring *vstr, char const * const label, dict const * const bit_labels,
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
	val &= (~0u << bits_unused);	// zeroize unused bits
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

ASN1_FORMATTER_PROTOTYPE(asn1_format_any) {
	if(label != NULL) {
		LA_ISPRINTF(vstr, indent, "%s: ", label);
	} else {
		LA_ISPRINTF(vstr, indent, "%s", "");
	}
	asn_sprintf(vstr, td, sptr, 1);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_NULL) {
	UNUSED(td);
	UNUSED(label);
	UNUSED(vstr);
	UNUSED(sptr);
	UNUSED(indent);
	// NOOP
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_label_only) {
	UNUSED(td);
	UNUSED(sptr);
	if(label != NULL) {
		LA_ISPRINTF(vstr, indent, "%s\n", label);
	}
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_ENUM) {
	long const value = *(long const *)sptr;
	char const *s = value2enum(td, value);
	if(s != NULL) {
		LA_ISPRINTF(vstr, indent, "%s: %s\n", label, s);
	} else {
		LA_ISPRINTF(vstr, indent, "%s: %ld\n", label, value);
	}
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_Deg) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " deg", 1, 0);
}
