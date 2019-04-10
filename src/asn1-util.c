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

#include <stdint.h>
#include <search.h>			// lfind()
#include <libacars/vstring.h>		// la_vstring
#include "asn1/asn_application.h"	// asn_TYPE_descriptor_t
#include "dumpvdl2.h"			// debug_print()
#include "asn1-util.h"			// asn_formatter_table

static int compare_fmtr(const void *k, const void *m) {
	asn_formatter_t *memb = (asn_formatter_t *)m;
	return(k == memb->type ? 0 : 1);
}

int asn1_decode_as(asn_TYPE_descriptor_t *td, void **struct_ptr, uint8_t *buf, int size) {
	asn_dec_rval_t rval;
	rval = uper_decode_complete(0, td, struct_ptr, buf, size);
	if(rval.code != RC_OK) {
		debug_print("uper_decode_complete failed: %d\n", rval.code);
		return -1;
	}
	if(rval.consumed < (size_t)size) {
		debug_print("uper_decode_complete left %zd unparsed octets\n", (size_t)size - rval.consumed);
		return (int)((size_t)size - rval.consumed);
	}
#ifdef DEBUG
	asn_fprint(stderr, td, *struct_ptr, 1);
#endif
	return 0;
}

void asn1_output(la_vstring *vstr, asn_formatter_t const * const asn1_formatter_table,
	size_t asn1_formatter_table_len, asn_TYPE_descriptor_t *td, const void *sptr, int indent) {
	if(td == NULL || sptr == NULL) return;
	asn_formatter_t *formatter = lfind(td, asn1_formatter_table, &asn1_formatter_table_len,
		sizeof(asn_formatter_t), &compare_fmtr);
	if(formatter != NULL) {
		(*formatter->format)(vstr, formatter->label, td, sptr, indent);
	} else {
		LA_ISPRINTF(vstr, indent, "-- Formatter for type %s not found, ASN.1 dump follows:\n", td->name);
		if(indent > 0) {
			LA_ISPRINTF(vstr, indent * 4, "%s", "");	// asn_sprintf does not indent the first line
		}
		asn_sprintf(vstr, td, sptr, indent+1);
		LA_ISPRINTF(vstr, indent, "%s", "-- ASN.1 dump end\n");
	}
}
