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

#include "output-common.h"              // fmtr_descriptor_t
#include "dumpvdl2.h"                   // octet_string_t
#include "dumpvdl2.pb-c.h"              // protobuf-c API

static bool fmtr_binary_supports_data_type(fmtr_input_type_t type) {
	return(type == FMTR_INTYPE_RAW_FRAME);
}

static octet_string_t *fmtr_binary_format_raw_frame(vdl2_msg_metadata *metadata, octet_string_t *frame) {
	ASSERT(metadata != NULL);
	ASSERT(frame != NULL);

	Dumpvdl2__Vdl2MsgMetadata__Timestamp ts = DUMPVDL2__VDL2_MSG_METADATA__TIMESTAMP__INIT;
	ts.tv_sec = metadata->burst_timestamp.tv_sec;
	ts.tv_usec = metadata->burst_timestamp.tv_usec;

	Dumpvdl2__Vdl2MsgMetadata m = DUMPVDL2__VDL2_MSG_METADATA__INIT;
	m.station_id = metadata->station_id;
	m.burst_timestamp = &ts;
	m.datalen_octets = metadata->datalen_octets;
	m.frequency = metadata->freq;
	m.frame_pwr_dbfs = metadata->frame_pwr_dbfs;
	m.nf_pwr_dbfs = metadata->nf_pwr_dbfs;
	m.idx = metadata->idx;
	m.num_fec_corrections = metadata->num_fec_corrections;
	m.ppm_error = metadata->ppm_error;
	m.synd_weight = metadata->synd_weight;
	m.version = metadata->version;

	Dumpvdl2__RawAvlcFrame f = DUMPVDL2__RAW_AVLC_FRAME__INIT;
	f.metadata = &m;
	f.data.data = frame->buf;
	f.data.len = frame->len;

	size_t len = dumpvdl2__raw_avlc_frame__get_packed_size(&f);
	void *buf = XCALLOC(sizeof(uint8_t), len);
	size_t packed_len = dumpvdl2__raw_avlc_frame__pack(&f, buf);
	debug_print(D_OUTPUT, "get_packed_size: %zu, pack_result_size: %zu\n", len, packed_len);

	return octet_string_new(buf, packed_len);
}

fmtr_descriptor_t fmtr_DEF_binary = {
	.name = "binary",
	.description = "Binary format, suitable for archiving raw frames",
	.format_decoded_msg = NULL,
	.format_raw_msg = fmtr_binary_format_raw_frame,
	.supports_data_type = fmtr_binary_supports_data_type,
	.output_format = OFMT_BINARY,
};
