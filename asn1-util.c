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

#include <stdint.h>
#include <search.h>
#include "asn1/asn_application.h"

#include "dumpvdl2.h"			// debug_print()
#include "asn1-util.h"

int asn1_decode_as(asn_TYPE_descriptor_t *td, void **struct_ptr, uint8_t *buf, int size) {
	asn_dec_rval_t rval;
	rval = uper_decode_complete(0, td, struct_ptr, buf, size);
	if(rval.code != RC_OK) {
		debug_print("uper_decode_complete failed: %d\n", rval.code);
		return -1;
	}
	if(rval.consumed < size) {
		debug_print("uper_decode_complete left %zd unparsed octets\n", size - rval.consumed);
		return size - rval.consumed;
	}
	if(DEBUG)
		asn_fprint(stderr, td, *struct_ptr, 1);
	return 0;
}
