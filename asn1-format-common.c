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

#include "asn1/asn_application.h"	// asn_TYPE_descriptor_t, asn_fprint
#include "asn1/INTEGER.h"		// asn_INTEGER_enum_map_t
#include "asn1-util.h"			// CAST_PTR, ASN1_FORMATTER_PROTOTYPE, IFPRINTF

char const *value2enum(asn_TYPE_descriptor_t *td, long const value) {
	if(td == NULL) return NULL;
	asn_INTEGER_enum_map_t const *enum_map = INTEGER_map_value2enum(td->specifics, value);
	if(enum_map == NULL) return NULL;
	return enum_map->enum_name;
}

void _format_INTEGER_with_unit(FILE *stream, char const * const label, asn_TYPE_descriptor_t *td,
	void const *sptr, int indent, char const * const unit, double multiplier, int decimal_places) {
	CAST_PTR(val, long *, sptr);
	IFPRINTF(stream, indent, "%s: %.*f%s\n", label, decimal_places, (double)(*val) * multiplier, unit);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_any) {
	if(label != NULL) {
		IFPRINTF(stream, indent, "%s: ", label);
	} else {
		IFPRINTF(stream, indent, "%s", "");
	}
	asn_fprint(stream, td, sptr, 1);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_NULL) {
	// NOOP
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_ENUM) {
	long const value = *(long const *)sptr;
	char const *s = value2enum(td, value);
	if(s != NULL) {
		IFPRINTF(stream, indent, "%s: %s\n", label, s);
	} else {
		IFPRINTF(stream, indent, "%s: %ld\n", label, value);
	}
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_Deg) {
	_format_INTEGER_with_unit(stream, label, td, sptr, indent, " deg", 1, 0);
}



