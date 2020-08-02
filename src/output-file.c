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

#include <stdio.h>                      // FILE, fprintf, fwrite, fputc
#include <string.h>                     // strcmp, strdup, strerror
#include <time.h>                       // gmtime_r, localtime_r, strftime
#include <errno.h>                      // errno
#include <arpa/inet.h>                  // htons
#include <glib.h>                       // g_async_queue_pop
#include "output-common.h"              // output_descriptor_t, output_qentry_t
#include "output-file.h"                // OUT_BINARY_FRAME_LEN_OCTETS, OUT_BINARY_FRAME_LEN_MAX
#include "kvargs.h"                     // kvargs
#include "dumpvdl2.h"                   // do_exit

typedef enum {
	ROT_NONE,
	ROT_HOURLY,
	ROT_DAILY
} out_file_rotation_mode;

typedef struct {
	FILE *fh;
	char *filename_prefix;
	char *extension;
	size_t prefix_len;
	struct tm current_tm;
	out_file_rotation_mode rotate;
} out_file_ctx_t;

static bool out_file_supports_format(output_format_t format) {
	return(format == OFMT_TEXT || format == OFMT_BINARY);
}

static void *out_file_configure(kvargs *kv) {
	ASSERT(kv != NULL);
	NEW(out_file_ctx_t, cfg);
	if(kvargs_get(kv, "path") == NULL) {
		fprintf(stderr, "output_file: path not specified\n");
		goto fail;
	}
	cfg->filename_prefix = strdup(kvargs_get(kv, "path"));
	debug_print(D_OUTPUT, "filename_prefix: %s\n", cfg->filename_prefix);
	char *rotate = kvargs_get(kv, "rotate");
	if(rotate != NULL) {
		if(!strcmp(rotate, "hourly")) {
			cfg->rotate = ROT_HOURLY;
		} else if(!strcmp(rotate, "daily")) {
			cfg->rotate = ROT_DAILY;
		} else {
			fprintf(stderr, "output_file: invalid rotation mode: %s\n", rotate);
			goto fail;
		}
	} else {
		cfg->rotate = ROT_NONE;
	}
	return cfg;
fail:
	XFREE(cfg);
	return NULL;
}

static int out_file_open(out_file_ctx_t *self) {
	char *filename = NULL;
	char *fmt = NULL;
	size_t tlen = 0;

	if(self->rotate != ROT_NONE) {
		time_t t = time(NULL);
		if(Config.utc == true) {
			gmtime_r(&t, &self->current_tm);
		} else {
			localtime_r(&t, &self->current_tm);
		}
		char suffix[16];
		if(self->rotate == ROT_HOURLY) {
			fmt = "_%Y%m%d_%H";
		} else if(self->rotate == ROT_DAILY) {
			fmt = "_%Y%m%d";
		}
		ASSERT(fmt != NULL);
		tlen = strftime(suffix, sizeof(suffix), fmt, &self->current_tm);
		if(tlen == 0) {
			fprintf(stderr, "open_outfile(): strfime returned 0\n");
			return -1;
		}
		filename = XCALLOC(self->prefix_len + tlen + 2, sizeof(uint8_t));
		sprintf(filename, "%s%s%s", self->filename_prefix, suffix, self->extension);
	} else {
		filename = strdup(self->filename_prefix);
	}

	if((self->fh = fopen(filename, "a+")) == NULL) {
		fprintf(stderr, "Could not open output file %s: %s\n", filename, strerror(errno));
		XFREE(filename);
		return -1;
	}
	XFREE(filename);
	return 0;
}

int out_file_init(out_file_ctx_t *self) {
	ASSERT(self != NULL);
	if(!strcmp(self->filename_prefix, "-")) {
		self->fh = stdout;
		self->rotate = ROT_NONE;
	} else {
		self->prefix_len = strlen(self->filename_prefix);
		if(self->rotate != ROT_NONE) {
			char *basename = strrchr(self->filename_prefix, '/');
			if(basename != NULL) {
				basename++;
			} else {
				basename = self->filename_prefix;
			}
			char *ext = strrchr(self->filename_prefix, '.');
			if(ext != NULL && (ext <= basename || ext[1] == '\0')) {
				ext = NULL;
			}
			if(ext) {
				self->extension = strdup(ext);
				*ext = '\0';
			} else {
				self->extension = strdup("");
			}
		}
		return out_file_open(self);
	}
	return 0;
}

static int out_file_rotate(out_file_ctx_t *self) {
	// FIXME: rotation should be driven by message timestamp, not the current timestamp
	struct tm new_tm;
	time_t t = time(NULL);
	if(Config.utc == true) {
		gmtime_r(&t, &new_tm);
	} else {
		localtime_r(&t, &new_tm);
	}
	if((self->rotate == ROT_HOURLY && new_tm.tm_hour != self->current_tm.tm_hour) ||
			(self->rotate == ROT_DAILY && new_tm.tm_mday != self->current_tm.tm_mday)) {
		fclose(self->fh);
		return out_file_open(self);
	}
	return 0;
}

static void out_file_produce_text(out_file_ctx_t *self, vdl2_msg_metadata *metadata, octet_string_t *msg) {
	ASSERT(msg != NULL);
	ASSERT(self->fh != NULL);
	UNUSED(metadata);
	// Subtract 1 to cut off NULL terminator
	fwrite(msg->buf, sizeof(uint8_t), msg->len - 1, self->fh);
	fputc('\n', self->fh);
	fflush(self->fh);
}

static void out_file_produce_binary(out_file_ctx_t *self, vdl2_msg_metadata *metadata, octet_string_t *msg) {
	ASSERT(msg != NULL);
	ASSERT(self->fh != NULL);
	UNUSED(metadata);

    size_t frame_len = msg->len + OUT_BINARY_FRAME_LEN_OCTETS;
    if (frame_len > OUT_BINARY_FRAME_LEN_MAX) {
        fprintf(stderr, "output_file: encoded payload too large: %zu > %d\n",
                frame_len, OUT_BINARY_FRAME_LEN_MAX);
        return;
    }
    uint16_t frame_len_be = htons((uint16_t)frame_len);
    debug_print(D_OUTPUT, "len: %zu frame_len_be: 0x%04x\n", frame_len, frame_len_be);
    fwrite(&frame_len_be, OUT_BINARY_FRAME_LEN_OCTETS, 1, self->fh);
    fwrite(msg->buf, sizeof(uint8_t), msg->len, self->fh);
    fflush(self->fh);
}

static void *out_file_thread(void *arg) {
	ASSERT(arg != NULL);
	CAST_PTR(ctx, output_ctx_t *, arg);
	CAST_PTR(self, out_file_ctx_t *, ctx->priv);

	if(out_file_init(self) < 0) {
		goto fail;
	}
	ctx->enabled = true;

	while(!do_exit) {
		output_qentry_t *q = (output_qentry_t *)g_async_queue_pop(ctx->q);
		ASSERT(q != NULL);
		if(self->rotate != ROT_NONE && out_file_rotate(self) < 0) {
			goto fail;
		}
		if(q->format == OFMT_TEXT) {
			out_file_produce_text(self, q->metadata, q->msg);
		} else if(q->format == OFMT_BINARY) {
			out_file_produce_binary(self, q->metadata, q->msg);
		}
		output_qentry_destroy(q);
	}

	fclose(self->fh);
	return NULL;

fail:
	ctx->enabled = false;
	fprintf(stderr, "output_file: could not write to '%s', output disabled\n", self->filename_prefix);
	return NULL;
}

output_descriptor_t out_DEF_file = {
	.name = "file",
	.start_routine = out_file_thread,
	.supports_format = out_file_supports_format,
	.configure = out_file_configure
};
