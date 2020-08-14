/*
 *  This file is a part of dumpvdl2
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

#include <stdio.h>                      // fprintf
#include <string.h>                     // strdup, strerror
#include <errno.h>                      // errno
#include <glib.h>                       // g_async_queue_pop
#include <zmq.h>                        // zmq_*
#include "output-common.h"              // output_descriptor_t, output_qentry_t, output_queue_drain
#include "kvargs.h"                     // kvargs
#include "dumpvdl2.h"                   // do_exit, option_descr_t

typedef enum {
	ZMQ_MODE_SERVER,
	ZMQ_MODE_CLIENT
} out_zmq_mode_t;

typedef struct {
	char *endpoint;
	void *zmq_ctx;
	void *zmq_sock;
	out_zmq_mode_t mode;
} out_zmq_ctx_t;

static bool out_zmq_supports_format(output_format_t format) {
	return(format == OFMT_TEXT || format == OFMT_PP_ACARS);
}

static void *out_zmq_configure(kvargs *kv) {
	ASSERT(kv != NULL);
	NEW(out_zmq_ctx_t, cfg);
	if(kvargs_get(kv, "endpoint") == NULL) {
		fprintf(stderr, "output_zmq: endpoint not specified\n");
		goto fail;
	}
	cfg->endpoint = strdup(kvargs_get(kv, "endpoint"));
	char *mode = NULL;
	if((mode = kvargs_get(kv, "mode")) == NULL) {
		fprintf(stderr, "output_zmq: mode not specified\n");
		goto fail;
	}
	if(!strcmp(mode, "server")) {
		cfg->mode = ZMQ_MODE_SERVER;
	} else if(!strcmp(mode, "client")) {
		cfg->mode = ZMQ_MODE_CLIENT;
	} else {
		fprintf(stderr, "output_zmq: mode '%s' is invalid; must be either 'client' or 'server'\n", mode);
		goto fail;
	}
	return cfg;
fail:
	XFREE(cfg);
	return NULL;
}

int out_zmq_init(out_zmq_ctx_t *self) {
	ASSERT(self != NULL);

	self->zmq_ctx = zmq_ctx_new();
	if(self->zmq_ctx == NULL) {
		fprintf(stderr, "output_zmq: failed to set up ZMQ context\n");
		return -1;
	}
	self->zmq_sock = zmq_socket(self->zmq_ctx, ZMQ_PUB);
	int rc = 0;
	if(self->mode == ZMQ_MODE_SERVER) {
		rc = zmq_bind(self->zmq_sock, self->endpoint);
	} else {    // ZMQ_MODE_CLIENT
		rc = zmq_connect(self->zmq_sock, self->endpoint);
	}
	if(rc < 0) {
		fprintf(stderr, "output_zmq: %s to %s failed: %s\n",
				self->mode == ZMQ_MODE_SERVER ? "bind" : "connect",
				self->endpoint, zmq_strerror(errno));
		return -1;
	}
	return 0;
}

static void out_zmq_produce_text(out_zmq_ctx_t *self, vdl2_msg_metadata *metadata, octet_string_t *msg) {
	UNUSED(metadata);
	ASSERT(msg != NULL);
	ASSERT(self->zmq_sock != 0);
	if(msg->len < 2) {
		return;
	}
	// Don't send the NULL terminator
	if(zmq_send(self->zmq_sock, msg->buf, msg->len - 1, 0) < 0) {
		debug_print(D_OUTPUT, "output_zmq: zmq_send error: %s", zmq_strerror(errno));
	}
}

static void *out_zmq_thread(void *arg) {
	ASSERT(arg != NULL);
	CAST_PTR(ctx, output_ctx_t *, arg);
	CAST_PTR(self, out_zmq_ctx_t *, ctx->priv);

	if(out_zmq_init(self) < 0) {
		goto fail;
	}

	while(1) {
		output_qentry_t *q = (output_qentry_t *)g_async_queue_pop(ctx->q);
		ASSERT(q != NULL);
		if(q->flags & OUT_FLAG_ORDERED_SHUTDOWN) {
			break;
		}
		if(q->format == OFMT_TEXT || q->format == OFMT_PP_ACARS) {
			out_zmq_produce_text(self, q->metadata, q->msg);
		}
		output_qentry_destroy(q);
	}

	fprintf(stderr, "output_zmq(%s): shutting down\n", self->endpoint);
	zmq_close(self->zmq_sock);
	zmq_ctx_destroy(self->zmq_ctx);
	ctx->active = false;
	return NULL;

fail:
	ctx->active = false;
	fprintf(stderr, "output_zmq: Could not %s to %s, deactivating output\n",
			self->mode == ZMQ_MODE_SERVER ? "bind" : "connect", self->endpoint);
	output_queue_drain(ctx->q);
	return NULL;
}

static const option_descr_t out_zmq_options[] = {
	{
		.name = "mode",
		.description = "Socket mode: client or server (required)"
	},
	{
		.name= "endpoint",
		.description = "Socket endpoint: tcp://address:port (required)"
	},
	{
		.name = NULL,
		.description = NULL
	}
};

output_descriptor_t out_DEF_zmq = {
	.name = "zmq",
	.description = "Output to a ZeroMQ publisher socket (as a server or a client)",
	.options = out_zmq_options,
	.start_routine = out_zmq_thread,
	.supports_format = out_zmq_supports_format,
	.configure = out_zmq_configure
};
