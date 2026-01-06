/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
 *  Copyright (c) 2017-2026 Tomasz Lemiech <szpajder@gmail.com>
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

#include <stdbool.h>
#include <string.h>                     // strdup
#include <libacars/libacars.h>          // la_proto_node
#include <libacars/vstring.h>           // la_vstring
#include "output-common.h"              // fmtr_descriptor_t
#include "dumpvdl2.h"                   // octet_string_t, Config
#include "acars.h"                      // acars_format_pp

static bool fmtr_pp_acars_supports_data_type(fmtr_input_type_t type) {
	return(type == FMTR_INTYPE_DECODED_FRAME);
}

static octet_string_t *fmtr_pp_acars_format_decoded_msg(vdl2_msg_metadata *metadata, la_proto_node *root) {
	ASSERT(root != NULL);
	UNUSED(metadata);

	la_vstring *vstr = acars_format_pp(root);
	if(vstr == NULL) {
		return NULL;
	}
	octet_string_t *ret = octet_string_new(vstr->str, vstr->len);
	la_vstring_destroy(vstr, false);
	return ret;
}

fmtr_descriptor_t fmtr_DEF_pp_acars = {
	.name = "pp_acars",
	.description = "One-line ACARS format accepted by Planeplotter via UDP",
	.format_decoded_msg = fmtr_pp_acars_format_decoded_msg,
	.format_raw_msg = NULL,
	.supports_data_type = fmtr_pp_acars_supports_data_type,
	.output_format = OFMT_PP_ACARS
};
