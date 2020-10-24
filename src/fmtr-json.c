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
#include <libacars/json.h>
#include "fmtr-json.h"
#include "output-common.h"              // fmtr_descriptor_t
#include "dumpvdl2.h"                   // octet_string_t, Config, DUMPVDL2_VERSION

// forward declarations
la_type_descriptor const la_DEF_vdl2_message;

void la_vdl2_format_json(la_vstring *vstr, void const *data) {
	ASSERT(vstr);
	ASSERT(data);

	vdl2_msg_metadata const *m = data;
	la_json_append_string(vstr, "app", "dumpvdl2");
	la_json_append_string(vstr, "ver", DUMPVDL2_VERSION);
	if(m->station_id != NULL) {
		la_json_append_string(vstr, "station", m->station_id);
	}

	la_json_object_start(vstr, "t");
	la_json_append_long(vstr, "sec", m->burst_timestamp.tv_sec);
	la_json_append_long(vstr, "usec", m->burst_timestamp.tv_usec);
	la_json_object_end(vstr);

	la_json_append_long(vstr, "freq", m->freq);
	la_json_append_long(vstr, "burst_len_octets", m->datalen_octets);
	la_json_append_long(vstr, "hdr_bits_fixed", m->synd_weight);
	la_json_append_long(vstr, "octets_corrected_by_fec", m->num_fec_corrections);
	la_json_append_long(vstr, "idx", m->idx);
	la_json_append_double(vstr, "sig_level", m->frame_pwr_dbfs);
	la_json_append_double(vstr, "noise_level", m->nf_pwr_dbfs);
	la_json_append_double(vstr, "freq_skew", m->ppm_error);
}

static bool fmtr_json_supports_data_type(fmtr_input_type_t type) {
	return(type == FMTR_INTYPE_DECODED_FRAME);
}

static octet_string_t *fmtr_json_format_decoded_msg(vdl2_msg_metadata *metadata, la_proto_node *root) {
	ASSERT(metadata != NULL);
	ASSERT(root != NULL);

	// prepend the metadata node the the tree (and destroy it afterwards)
	la_proto_node *vdl2_msg = la_proto_node_new();
	vdl2_msg->td = &la_DEF_vdl2_message;
	vdl2_msg->data = metadata;
	vdl2_msg->next = root;

	la_vstring *vstr = la_proto_tree_format_json(NULL, vdl2_msg);
	octet_string_t *ret = octet_string_new(vstr->str, vstr->len);
	la_vstring_destroy(vstr, false);
	XFREE(vdl2_msg);
	return ret;
}

la_type_descriptor const la_DEF_vdl2_message = {
	.format_text = NULL,
	.format_json = la_vdl2_format_json,
	.json_key = "vdl2",
	.destroy = NULL
};

fmtr_descriptor_t fmtr_DEF_json = {
	.name = "json",
	.description = "Javascript object notation",
	.format_decoded_msg = fmtr_json_format_decoded_msg,
	.format_raw_msg = NULL,
	.supports_data_type = fmtr_json_supports_data_type,
	.output_format = OFMT_JSON
};
