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

#ifndef _OUTPUT_COMMON_H
#define _OUTPUT_COMMON_H

#include <pthread.h>                    // pthread_t
#include <glib.h>                       // g_async_queue
#include <libacars/libacars.h>          // la_proto_node
#include <libacars/list.h>              // la_list
#include "dumpvdl2.h"                   // octet_string_t
#include "kvargs.h"                     // kvargs

// Metadata of a VDL2 frame
typedef struct {
	char *station_id;                   // textual identifier of the receiving station
	uint32_t freq;                      // channel frequency
	uint32_t synd_weight;               // number of bit errors corrected in the burst header
	uint32_t datalen_octets;            // burst length (octets)
	float frame_pwr_dbfs;               // received signal level (in dBFS)
	float nf_pwr_dbfs;                  // noise floor level (in dBFS)
	float ppm_error;                    // burst carrier frequency skew (in ppm)
	int version;                        // metadata version
	int num_fec_corrections;            // number of octets corrected by FEC
	int idx;                            // message number
	struct timeval burst_timestamp;     // receive timestamp of the VDL2 burst (not message!)
} vdl2_msg_metadata;

// Data type on formatter input
typedef enum {
	FMTR_INTYPE_UNKNOWN           = 0,
	FMTR_INTYPE_DECODED_FRAME     = 1,
	FMTR_INTYPE_RAW_FRAME         = 2
} fmtr_input_type_t;

// Output formats
typedef enum {
	OFMT_UNKNOWN    = 0,
	OFMT_TEXT       = 1
} output_format_t;

typedef octet_string_t* (fmt_decoded_fun_t)(vdl2_msg_metadata *metadata, la_proto_node *root);
typedef octet_string_t* (fmt_raw_fun_t)(vdl2_msg_metadata *metadata, octet_string_t *msg);
typedef bool (intype_check_fun_t)(fmtr_input_type_t);

// Frame formatter descriptor
typedef struct {
    fmt_decoded_fun_t *format_decoded_msg;
    fmt_raw_fun_t *format_raw_msg;
    intype_check_fun_t *supports_data_type;
    output_format_t output_format;
} fmtr_descriptor_t;

// Frame formatter instance
typedef struct {
	fmtr_descriptor_t *td;           // type descriptor of the formatter used
	fmtr_input_type_t intype;        // what kind of data to pass to the input of this formatter
	la_list *outputs;                // list of output descriptors where the formatted message should be sent
} fmtr_instance_t;

typedef void* (pthread_start_fun_t)(void *);
typedef bool (output_format_check_fun_t)(output_format_t);
typedef void* (output_configure_fun_t)(kvargs *);

// Output descriptor
typedef struct {
	char *name;
	pthread_start_fun_t *start_routine;
	output_format_check_fun_t *supports_format;
	output_configure_fun_t *configure;
} output_descriptor_t;

// Output instance context (passed to the thread routine)
typedef struct {
	GAsyncQueue *q;                 // input queue
	void *priv;                     // output instance context (private)
	output_format_t format;         // format of the data fed into the output
	bool enabled;                   // output is ready to process messages
} output_ctx_t;

// Output instance
typedef struct {
	output_descriptor_t *td;        // type descriptor of the output
	pthread_t *output_thread;       // thread of this output instance
	output_ctx_t *ctx;              // context data for the thread
} output_instance_t;

// Messages passed via output queues
typedef struct {
	octet_string_t *msg;            // formatted message
	vdl2_msg_metadata *metadata;    // message metadata
	output_format_t format;         // format of the data stored in msg
} output_qentry_t;

fmtr_input_type_t fmtr_input_type_from_string(char const * const str);
fmtr_descriptor_t *fmtr_descriptor_get(output_format_t const fmt);
fmtr_instance_t *fmtr_instance_new(fmtr_descriptor_t *fmttd, fmtr_input_type_t intype);

output_format_t output_format_from_string(char const * const str);
output_descriptor_t *output_descriptor_get(char const * const output_name);
output_instance_t *output_instance_new(output_descriptor_t *outtd, output_format_t format, void *priv);
output_qentry_t *output_qentry_copy(output_qentry_t const * const q);
void output_qentry_destroy(output_qentry_t *q);

vdl2_msg_metadata *vdl2_msg_metadata_copy(vdl2_msg_metadata const * const m);
void vdl2_msg_metadata_destroy(vdl2_msg_metadata *m);

#endif // !_OUTPUT_COMMON_H
