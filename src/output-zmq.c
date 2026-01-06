/*
 *  This file is a part of dumpvdl2
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

#include <stdio.h>                      // fprintf
#include <string.h>                     // strdup, strerror
#include <errno.h>                      // errno
#include <zmq.h>                        // zmq_*
#include "config.h"                     // LIBZMQ_VER_*
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
	return(format == OFMT_TEXT || format == OFMT_JSON || format == OFMT_PP_ACARS);
}

static void *out_zmq_configure(kvargs *kv) {
	ASSERT(kv != NULL);
	NEW(out_zmq_ctx_t, cfg);

	int major, minor, patch;
	zmq_version(&major, &minor, &patch);
	if((major * 1000000 + minor * 1000 + patch) < (LIBZMQ_VER_MAJOR_MIN * 1000000 + LIBZMQ_VER_MINOR_MIN * 1000 + LIBZMQ_VER_PATCH_MIN)) {
		fprintf(stderr, "output_zmq: error: libzmq library version %d.%d.%d is too old; at least %d.%d.%d is required\n",
				major, minor, patch,
				LIBZMQ_VER_MAJOR_MIN, LIBZMQ_VER_MINOR_MIN, LIBZMQ_VER_PATCH_MIN);
		goto fail;
	}
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

static int out_zmq_init(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_zmq_ctx_t *self = selfptr;

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
	rc = zmq_setsockopt(self->zmq_sock, ZMQ_SNDHWM, &Config.output_queue_hwm,
			sizeof(Config.output_queue_hwm));
	if(rc < 0) {
		fprintf(stderr, "output_zmq: could not set ZMQ_SNDHWM option for socket: %s\n",
				zmq_strerror(errno));
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
	if(zmq_send(self->zmq_sock, msg->buf, msg->len, 0) < 0) {
		debug_print(D_OUTPUT, "output_zmq: zmq_send error: %s", zmq_strerror(errno));
	}
}

static int out_zmq_produce(void *selfptr, output_format_t format, vdl2_msg_metadata *metadata, octet_string_t *msg) {
	ASSERT(selfptr != NULL);
	out_zmq_ctx_t *self = selfptr;
	if(format == OFMT_TEXT || format == OFMT_JSON || format == OFMT_PP_ACARS) {
		out_zmq_produce_text(self, metadata, msg);
	}
	return 0;
}

static void out_zmq_handle_shutdown(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_zmq_ctx_t *self = selfptr;
	fprintf(stderr, "output_zmq(%s): shutting down\n", self->endpoint);
	zmq_close(self->zmq_sock);
	zmq_ctx_destroy(self->zmq_ctx);
}

static void out_zmq_handle_failure(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_zmq_ctx_t *self = selfptr;
	fprintf(stderr, "output_zmq: Could not %s to %s, deactivating output\n",
			self->mode == ZMQ_MODE_SERVER ? "bind" : "connect", self->endpoint);
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
	.supports_format = out_zmq_supports_format,
	.configure = out_zmq_configure,
	.init = out_zmq_init,
	.produce = out_zmq_produce,
	.handle_shutdown = out_zmq_handle_shutdown,
	.handle_failure = out_zmq_handle_failure
};
