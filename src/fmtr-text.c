/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
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

#include <stdbool.h>
#include <libacars/libacars.h>          // la_proto_node
#include <libacars/vstring.h>           // la_vstring
#include "fmtr-text.h"
#include "output-common.h"              // fmtr_descriptor_t
#include "dumpvdl2.h"                   // octet_string_t

static bool fmtr_text_supports_data_type(fmtr_input_type_t type) {
	return(type == (fmtr_input_type_t)OFMT_TEXT);
}

static octet_string_t *fmtr_text_format_decoded_msg(vdl2_msg_metadata *metadata, la_proto_node *root) {
	ASSERT(metadata != NULL);
	ASSERT(root != NULL);

	// TODO: print metadata here rather than in avlc_format_text
	la_vstring *vstr = la_proto_tree_format_text(NULL, root);
	octet_string_t *ret = octet_string_new(vstr->str, vstr->len + 1);     // +1 for NULL terminator
	la_vstring_destroy(vstr, false);
	return ret;
}

fmtr_descriptor_t fmtr_DEF_text = {
	.format_decoded_msg = fmtr_text_format_decoded_msg,
	.format_raw_msg = NULL,
	.supports_data_type = fmtr_text_supports_data_type,
	.output_format = OFMT_TEXT,
};
