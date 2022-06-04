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

#include <stdint.h>
#include <stdio.h>                  // FILE, fopen, fclose, fread
#include <string.h>                 // memcpy
#include <arpa/inet.h>              // ntohs
#include "dumpvdl2.pb-c.h"
#include "output-common.h"          // vdl2_msg_metadata
#include "output-file.h"            // OUT_BINARY_FRAME_LEN_MAX, OUT_FILE_FRAME_LEN_OCTETS
#include "decode.h"                 // avlc_decoder_queue_push
#include "dumpvdl2.h"               // ASSERT, do_exit

#define BUF_SIZE (3 * OUT_BINARY_FRAME_LEN_MAX)
#define READ_SIZE (2 * OUT_BINARY_FRAME_LEN_MAX)

static int process_frame(uint8_t *buf, size_t len) {
	ASSERT(buf != NULL);
	Dumpvdl2__RawAvlcFrame *f =
		dumpvdl2__raw_avlc_frame__unpack(NULL, len, buf);
	if(f == NULL) {
		fprintf(stderr, "Failed to unpack message\n");
		return -1;
	}
	if(f->metadata == NULL) {
		fprintf(stderr, "No metadata in frame, skipping\n");
		return 0;
	}
	if(f->data.data == NULL || f->data.len < 1) {
		return 0;
	}
	Dumpvdl2__Vdl2MsgMetadata *m = f->metadata;
	if(m->burst_timestamp == NULL) {
		fprintf(stderr, "No timestamp in frame metadata, skipping\n");
		return 0;
	}
	NEW(vdl2_msg_metadata, metadata);
	metadata->version = m->version;;
	metadata->freq = m->frequency;
	metadata->frame_pwr_dbfs = m->frame_pwr_dbfs;
	metadata->nf_pwr_dbfs = m->nf_pwr_dbfs;
	metadata->ppm_error = m->ppm_error;
	metadata->burst_timestamp.tv_sec = m->burst_timestamp->tv_sec;
	metadata->burst_timestamp.tv_usec = m->burst_timestamp->tv_usec;
	metadata->datalen_octets = m->datalen_octets;
	metadata->synd_weight = m->synd_weight;
	metadata->num_fec_corrections = m->num_fec_corrections;
	metadata->idx = m->idx;

	uint8_t *copy = XCALLOC(f->data.len, sizeof(uint8_t));
	memcpy(copy, f->data.data, f->data.len);
	int flags = 0;
	avlc_decoder_queue_push(metadata, octet_string_new(copy, f->data.len), flags);
	dumpvdl2__raw_avlc_frame__free_unpacked(f, NULL);
	return 0;
}

int input_raw_frames_file_process(char const *file) {
	ASSERT(file != NULL);
	FILE *fh = fopen(file, "r");
	if (fh == NULL) {
		perror("fopen()");
		return 2;
	}
	int ret = 0;
	size_t available = 0, offset = 0, i = 0;
	size_t frame_len = 0;
	uint8_t buf[BUF_SIZE];
	while(do_exit == 0 && (available = fread(buf + offset, sizeof(uint8_t), READ_SIZE, fh)) > 0) {
		available += offset;
		i = offset = 0;
		while(available > 0) {
			if(available >= OUT_BINARY_FRAME_LEN_OCTETS) {      // we can at least read frame length
				frame_len = ntohs(*(uint16_t *)(buf + i));
				if(frame_len < OUT_BINARY_FRAME_LEN_OCTETS + 1) {
					fprintf(stderr, "Frame too short: %zu\n", frame_len);
					ret = 3;
					goto cleanup;
				}
			} else {
				frame_len = (size_t)(-1);                       // set this to impossibly high value
			}                                                   // to force loading next batch of data from the file
			if(available >= frame_len) {                        // whole frame can be read from the current buffer
				if(process_frame(buf + i + OUT_BINARY_FRAME_LEN_OCTETS,
							frame_len - OUT_BINARY_FRAME_LEN_OCTETS) != 0) {
					ret = 3;
					goto cleanup;
				}
				available -= frame_len;
				i += frame_len;
			} else {
				debug_print(D_MISC, "partial: need %zu octets, have %zu\n", frame_len, available);
				memcpy(buf, buf + i, available);                // move the partial frame to the start of the buffer
				offset = available;                             // save the amount of unprocessed data still in the buffer
				available = 0;                                  // exit the inner loop
			}
		}
	}
	if(offset > 0) {
		fprintf(stderr, "Input file is truncated\n");
		ret = 3;
		goto cleanup;
	}
cleanup:
	fclose(fh);
	return ret;
}
