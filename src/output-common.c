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

#include <string.h>             // memset, strcmp, strdup
#include <glib.h>               // g_async_queue_new
#include "config.h"             // WITH_*
#include "dumpvdl2.h"           // NEW, ASSERT
#include "output-common.h"

#include "fmtr-text.h"          // fmtr_DEF_text
#include "fmtr-pp_acars.h"      // fmtr_DEF_pp_acars
#ifdef WITH_PROTOBUF_C
#include "fmtr-binary.h"        // fmtr_DEF_binary
#endif

#include "output-file.h"        // out_DEF_file
#include "output-udp.h"         // out_DEF_udp
#ifdef WITH_ZMQ
#include "output-zmq.h"         // out_DEF_zmq
#endif

static dict const fmtr_intype_names[] = {
	{
		.id = FMTR_INTYPE_DECODED_FRAME,
		.val = &(option_descr_t) {
			.name= "decoded",
			.description = "Output decoded frames"
		}
	},
	{
		.id = FMTR_INTYPE_RAW_FRAME,
		.val = &(option_descr_t) {
			.name= "raw",
			.description = "Output undecoded AVLC frame as raw bytes"
		}
	},
	{
		.id = FMTR_INTYPE_UNKNOWN,
		.val = NULL
	}
};

static dict const fmtr_descriptors[] = {
	{ .id = OFMT_TEXT,                  .val = &fmtr_DEF_text },
	{ .id = OFMT_PP_ACARS,              .val = &fmtr_DEF_pp_acars },
#ifdef WITH_PROTOBUF_C
	{ .id = OFMT_BINARY,                .val = &fmtr_DEF_binary },
#endif
	{ .id = OFMT_UNKNOWN,               .val = NULL }
};

static output_descriptor_t * output_descriptors[] = {
	&out_DEF_file,
	&out_DEF_udp,
#ifdef WITH_ZMQ
	&out_DEF_zmq,
#endif
	NULL
};

fmtr_input_type_t fmtr_input_type_from_string(char const * const str) {
	for (dict const *d = fmtr_intype_names; d->val != NULL; d++) {
		if (!strcmp(str, ((option_descr_t *)d->val)->name)) {
			return d->id;
		}
	}
	return FMTR_INTYPE_UNKNOWN;
}

fmtr_descriptor_t *fmtr_descriptor_get(output_format_t const fmt) {
	return dict_search(fmtr_descriptors, fmt);
}

fmtr_instance_t *fmtr_instance_new(fmtr_descriptor_t *fmttd, fmtr_input_type_t intype) {
	ASSERT(fmttd != NULL);
	NEW(fmtr_instance_t, fmtr);
	fmtr->td = fmttd;
	fmtr->intype = intype;
	fmtr->outputs = NULL;
	return fmtr;
}

output_format_t output_format_from_string(char const * const str) {
	for (dict const *d = fmtr_descriptors; d->val != NULL; d++) {
		if (!strcmp(str, ((fmtr_descriptor_t *)d->val)->name)) {
			return d->id;
		}
	}
	return OFMT_UNKNOWN;
}

output_descriptor_t *output_descriptor_get(char const * const output_name) {
	if(output_name == NULL) {
		return NULL;
	}
	for(output_descriptor_t **outd = output_descriptors; *outd != NULL; outd++) {
		if(!strcmp(output_name, (*outd)->name)) {
			return *outd;
		}
	}
	return NULL;
}

output_instance_t *output_instance_new(output_descriptor_t *outtd, output_format_t format, void *priv) {
	ASSERT(outtd != NULL);
	NEW(output_ctx_t, ctx);
	ctx->q = g_async_queue_new();
	ctx->format = format;
	ctx->priv = priv;
	ctx->active = true;
	NEW(output_instance_t, output);
	output->td = outtd;
	output->ctx = ctx;
	output->output_thread = XCALLOC(1, sizeof(pthread_t));
	return output;
}

output_qentry_t *output_qentry_copy(output_qentry_t const * const q) {
	ASSERT(q != NULL);

	NEW(output_qentry_t, copy);
	if(q->msg != NULL) {
		copy->msg = octet_string_copy(q->msg);
	}
	if(q->metadata != NULL) {
		copy->metadata = vdl2_msg_metadata_copy(q->metadata);
	}
	copy->format = q->format;
	copy->flags = q->flags;
	return copy;
}

void output_qentry_destroy(output_qentry_t *q) {
	if(q == NULL) {
		return;
	}
	octet_string_destroy(q->msg);
	vdl2_msg_metadata_destroy(q->metadata);
	XFREE(q);
}

void output_queue_drain(GAsyncQueue *q) {
	ASSERT(q != NULL);
	g_async_queue_lock(q);
	while(g_async_queue_length_unlocked(q) > 0) {
		output_qentry_t *qentry = (output_qentry_t *)g_async_queue_pop_unlocked(q);
		output_qentry_destroy(qentry);
	}
	g_async_queue_unlock(q);
}

vdl2_msg_metadata *vdl2_msg_metadata_copy(vdl2_msg_metadata const * const m) {
	ASSERT(m != NULL);
	NEW(vdl2_msg_metadata, copy);
	memcpy(copy, m, sizeof(vdl2_msg_metadata));
	if(m->station_id != NULL) {
		copy->station_id = strdup(m->station_id);
	}
	return copy;
}

void vdl2_msg_metadata_destroy(vdl2_msg_metadata *m) {
	if(m == NULL) {
		return;
	}
	XFREE(m->station_id);
	XFREE(m);
}

void output_usage() {
	fprintf(stderr, "\n<output_specifier> is a parameter of the --output option. It has the following syntax:\n\n");
	fprintf(stderr, "%*s<what_to_output>:<output_format>:<output_type>:<output_parameters>\n\n", IND(1), "");
	fprintf(stderr, "where:\n");
	fprintf(stderr, "\n%*s<what_to_output> specifies what data should be sent to the output:\n\n", IND(1), "");
	for(dict const *p = fmtr_intype_names; p->val != NULL; p++) {
		CAST_PTR(n, option_descr_t *, p->val);
		describe_option(n->name, n->description, 2);
	}
	fprintf(stderr, "\n%*s<output_format> specifies how the output should be formatted:\n\n", IND(1), "");
	for(dict const *p = fmtr_descriptors; p->val != NULL; p++) {
		CAST_PTR(n, fmtr_descriptor_t *, p->val);
		describe_option(n->name, n->description, 2);
	}
	fprintf(stderr, "\n%*s<output_type> specifies the type of the output:\n\n", IND(1), "");
	for(output_descriptor_t **od = output_descriptors; *od != NULL; od++) {
		describe_option((*od)->name, (*od)->description, 2);
	}
	fprintf(stderr,
			"\n%*s<output_parameters> - specifies detailed output options with a syntax of: param1=value1,param2=value2,...\n",
			IND(1), ""
		   );
	for(output_descriptor_t **od = output_descriptors; *od != NULL; od++) {
		fprintf(stderr, "\nParameters for output type '%s':\n\n", (*od)->name);
		if((*od)->options != NULL) {
			for(option_descr_t const *opt = (*od)->options; opt->name != NULL; opt++) {
				describe_option(opt->name, opt->description, 2);
			}
		}
	}
	fprintf(stderr, "\n");
}
