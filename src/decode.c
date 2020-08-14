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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <glib.h>                   // GAsyncQueue, g_async_queue_*
#include <math.h>                   // log10f
#include <libacars/libacars.h>      // la_proto_node, la_proto_tree_destroy()
#include <libacars/reassembly.h>    // la_reasm_ctx, la_reasm_ctx_new()
#include "config.h"
#ifdef WITH_STATSD
#include <sys/time.h>
#endif
#include "decode.h"                 // avlc_decoder_queue
#include "output-common.h"
#include "dumpvdl2.h"
#include "avlc.h"                   // avlc_frame_qentry_t

// Reasonable limits for transmission lengths in bits
// This is to avoid blocking the decoder in DEC_DATA for a long time
// in case when the transmission length field in the header gets
// decoded wrongly.
// This applies when header decoded OK without error corrections
#define MAX_FRAME_LENGTH 0x3FFF

// This applies when there were some bits corrected
#define MAX_FRAME_LENGTH_CORRECTED 0x1FFF

#define LFSR_IV 0x6959u

static GAsyncQueue *avlc_decoder_queue;

static uint32_t const H[HDRFECLEN] = {
	0b0000000011111111111110000,
	0b0011111100001111111101000,
	0b1100011100110000111100100,
	0b1101101101010011001100010,
	0b0110100111100101010100001
};

static uint32_t const syndtable[1<<HDRFECLEN] = {
	0b0000000000000000000000000,
	0b0000000000000000000000001,
	0b0000000000000000000000010,
	0b0100000000000000000000100,
	0b0000000000000000000000100,
	0b0100000000000000000000010,
	0b1000000000000000000000000,
	0b0100000000000000000000000,
	0b0000000000000000000001000,
	0b0010000000000000000000000,
	0b0001000000000000000000000,
	0b0000100000000000000000000,
	0b0000010000000000000000000,
	0b1000100000000000000000000,
	0b0000001000000000000000000,
	0b0000000100000000000000000,
	0b0000000000000000000010000,
	0b0000000010000000000000000,
	0b0100000000100000000000000,
	0b0000000001000000000000000,
	0b0100000001000000000000000,
	0b0000000000100000000000000,
	0b0000000000010000000000000,
	0b1000000010000000000000000,
	0b0000000000001000000000000,
	0b0000000000000100000000000,
	0b0000000000000010000000000,
	0b0000000000000001000000000,
	0b0000000000000000100000000,
	0b0000000000000000010000000,
	0b0000000000000000001000000,
	0b0000000000000000000100000,
};

static uint32_t const synd_weight[1<<HDRFECLEN] = {
	0, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 2, 1, 2, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1
};

uint32_t parity(uint32_t v) {
	uint32_t parity = 0;
	while (v) {
		parity = !parity;
		v = v & (v - 1);
	}
	return parity;
}

uint32_t decode_header(uint32_t * const r) {
	uint32_t syndrome = 0u, row = 0u;
	int i;
	for(i = 0; i < HDRFECLEN; i++) {
		row = *r & H[i];
		syndrome |= (parity(row)) << (HDRFECLEN - 1 - i);
	}
	debug_print(D_BURST, "received: 0x%x syndrome: 0x%x error: 0x%x, decoded: 0x%x\n",
			*r, syndrome, syndtable[syndrome], *r ^ syndtable[syndrome]);
	*r ^= syndtable[syndrome];
	return syndrome;
}

int get_fec_octetcount(uint32_t len) {
	if(len < 3)
		return 0;
	else if(len < 31)
		return 2;
	else if(len < 68)
		return 4;
	else
		return 6;
}

static int deinterleave(uint8_t *in, uint32_t len, uint32_t rows, uint32_t cols, uint8_t out[][cols], uint32_t fillwidth, uint32_t offset) {
	if(rows == 0 || cols == 0 || fillwidth == 0)
		return -1;
	uint32_t last_row_len = len % fillwidth;
	if(last_row_len == 0) last_row_len = fillwidth;
	if(fillwidth + offset > cols)                               // fillwidth or offset too large
		return -2;
	if(len > rows * fillwidth)                                  // result won't fit
		return -3;
	if(rows > 1 && len - last_row_len < (rows - 1) * fillwidth) // not enough data to fill requested width
		return -4;
	if(last_row_len == 0 && len / fillwidth < rows)             // not enough data to fill requested number of rows
		return -5;
	uint32_t row = 0, col = offset;
	last_row_len += offset;
	for(uint32_t i = 0; i < len; i++) {
		if(row == rows - 1 && col >= last_row_len) {
			out[row][col] = 0x00;
			row = 0;
			col++;
		}
		out[row++][col] = in[i];
		if(row == rows) {
			row = 0;
			col++;
		}
	}
	return 0;
}

void avlc_decoder_queue_push(vdl2_msg_metadata *metadata, octet_string_t *frame, int flags) {
	NEW(avlc_frame_qentry_t, qentry);
	qentry->metadata = metadata;
	qentry->frame = frame;
	qentry->flags = flags;
	g_async_queue_push(avlc_decoder_queue, qentry);
}

static void decode_frame(vdl2_channel_t const *const v,
		int const frame_num, uint8_t *buf,
		size_t const len) {
	NEW(vdl2_msg_metadata, metadata);
	metadata->version = 1;
	metadata->station_id = Config.station_id;
	metadata->freq = v->freq;
	metadata->frame_pwr_dbfs = 10.0f * log10f(v->frame_pwr);
	metadata->nf_pwr_dbfs = 20.0f * log10f(v->mag_nf + 0.001f);
	metadata->ppm_error = v->ppm_error;
	metadata->burst_timestamp.tv_sec = v->burst_timestamp.tv_sec;
	metadata->burst_timestamp.tv_usec = v->burst_timestamp.tv_usec;
	metadata->datalen_octets = v->datalen_octets;
	metadata->synd_weight = synd_weight[v->syndrome];
	metadata->num_fec_corrections = v->num_fec_corrections;
	metadata->idx = frame_num;
	int flags = 0;

	uint8_t *copy = XCALLOC(len, sizeof(uint8_t));
	memcpy(copy, buf, len);
	avlc_decoder_queue_push(metadata, octet_string_new(copy, len), flags);
}

void decode_vdl_frame(vdl2_channel_t *v) {
	switch(v->decoder_state) {
		case DEC_HEADER:
			v->lfsr = LFSR_IV;
			bitstream_descramble(v->bs, &v->lfsr);
			uint32_t header;
			if(bitstream_read_word_msbfirst(v->bs, &header, HEADER_LEN) < 0) {
				debug_print(D_BURST, "Could not read header from bitstream\n");
				statsd_increment_per_channel(v->freq, "decoder.errors.no_header");
				v->decoder_state = DEC_IDLE;
				return;
			}
			// force bits of reserved symbol to 0 to improve chances of successful decode
			header &= ONES(TRLEN+HDRFECLEN);
			v->syndrome = decode_header(&header);
			if(v->syndrome == 0) {
				statsd_increment_per_channel(v->freq, "decoder.crc.good");
			}
			// sanity check - reserved symbol bits shall still be set to 0
			if((header & ONES(TRLEN+HDRFECLEN)) != header) {
				debug_print(D_BURST, "Rejecting decoded header with non-zero reserved bits\n");
				statsd_increment_per_channel(v->freq, "decoder.crc.bad");
				v->decoder_state = DEC_IDLE;
				return;
			}
			header >>= HDRFECLEN;
			v->datalen = reverse(header & ONES(TRLEN), TRLEN);
			// Reject payloads with unreasonably large length (in theory longer frames are allowed but in practice
			// it does not happen - usually it means we've locked on something which is not a preamble. It's safer
			// to reject it rather than to block the decoder in DEC_DATA state and reading garbage for a long time,
			// possibly overlooking valid frames.
			if((v->syndrome != 0 && v->datalen > MAX_FRAME_LENGTH_CORRECTED) || v->datalen > MAX_FRAME_LENGTH) {
				debug_print(D_BURST, "v->datalen=%u v->syndrome=%u - frame rejected\n", v->datalen, v->syndrome);
				statsd_increment_per_channel(v->freq, "decoder.errors.too_long");
				v->decoder_state = DEC_IDLE;
				return;
			}
			v->datalen_octets = v->datalen / 8;
			if(v->datalen % 8 != 0)
				v->datalen_octets++;
			v->num_blocks = v->datalen_octets / RS_K;
			v->fec_octets = v->num_blocks * (RS_N - RS_K);
			v->last_block_len_octets = v->datalen_octets % RS_K;
			if(v->last_block_len_octets != 0)
				v->num_blocks++;

			v->fec_octets += get_fec_octetcount(v->last_block_len_octets);

			debug_print(D_BURST, "Data length: %u (0x%x) bits (%u octets), num_blocks=%u, last_block_len_octets=%u fec_octets=%u\n",
					v->datalen, v->datalen, v->datalen_octets, v->num_blocks, v->last_block_len_octets, v->fec_octets);

			if(v->fec_octets == 0) {
				debug_print(D_BURST, "fec_octets is 0 which means the frame is unreasonably short\n");
				statsd_increment_per_channel(v->freq, "decoder.errors.no_fec");
				v->decoder_state = DEC_IDLE;
				return;
			}
			v->requested_bits = 8 * (v->datalen_octets + v->fec_octets);
			v->decoder_state = DEC_DATA;
			return;
		case DEC_DATA:
#ifdef WITH_STATSD
			gettimeofday(&v->tstart, NULL);
#endif
			bitstream_descramble(v->bs, &v->lfsr);
			uint8_t *data = XCALLOC(v->datalen_octets, sizeof(uint8_t));
			uint8_t *fec = XCALLOC(v->fec_octets, sizeof(uint8_t));
			if(bitstream_read_lsbfirst(v->bs, data, v->datalen_octets, 8) < 0) {
				debug_print(D_BURST, "Frame data truncated\n");
				statsd_increment_per_channel(v->freq, "decoder.errors.data_truncated");
				goto cleanup;
			}
			if(bitstream_read_lsbfirst(v->bs, fec, v->fec_octets, 8) < 0) {
				debug_print(D_BURST, "FEC data truncated\n");
				statsd_increment_per_channel(v->freq, "decoder.errors.fec_truncated");
				goto cleanup;
			}
			debug_print_buf_hex(D_BURST_DETAIL, data, v->datalen_octets, "Data:\n");
			debug_print_buf_hex(D_BURST_DETAIL, fec, v->fec_octets, "FEC:\n") ;
			{
				uint8_t rs_tab[v->num_blocks][RS_N];
				memset(rs_tab, 0, sizeof(uint8_t[v->num_blocks][RS_N]));
				int ret;
				if((ret = deinterleave(data, v->datalen_octets, v->num_blocks, RS_N, rs_tab, RS_K, 0)) < 0) {
					debug_print(D_BURST, "Deinterleaver failed with error %d\n", ret);
					statsd_increment_per_channel(v->freq, "decoder.errors.deinterleave_data");
					goto cleanup;
				}

				// if last block is < 3 bytes long, no FEC is done on it, so we should not write FEC bytes into the last row
				uint32_t fec_rows = v->num_blocks;
				if(get_fec_octetcount(v->last_block_len_octets) == 0)
					fec_rows--;

				if((ret = deinterleave(fec, v->fec_octets, fec_rows, RS_N, rs_tab, RS_N - RS_K, RS_K)) < 0) {
					debug_print(D_BURST, "Deinterleaver failed with error %d\n", ret);
					statsd_increment_per_channel(v->freq, "decoder.errors.deinterleave_fec");
					goto cleanup;
				}
#ifdef DEBUG
				debug_print(D_BURST_DETAIL, "Deinterleaved blocks:\n");
				for(uint32_t r = 0; r < v->num_blocks; r++) {
					debug_print_buf_hex(D_BURST_DETAIL, rs_tab[r], RS_N, "Block %d:\n", r);
				}
#endif
				bitstream_reset(v->bs);
				for(uint32_t r = 0; r < v->num_blocks; r++) {
					statsd_increment_per_channel(v->freq, "decoder.blocks.processed");
					int num_fec_octets = RS_N - RS_K;   // full block
					if(r == v->num_blocks - 1) {        // final, partial block
						num_fec_octets = get_fec_octetcount(v->last_block_len_octets);
					}
					ret = rs_verify((uint8_t *)&rs_tab[r], num_fec_octets);
					debug_print(D_BURST, "Block %d FEC: %d\n", r, ret);
					if(ret < 0) {
						debug_print(D_BURST, "FEC check failed\n");
						statsd_increment_per_channel(v->freq, "decoder.errors.fec_bad");
						goto cleanup;
					} else {
						statsd_increment_per_channel(v->freq, "decoder.blocks.fec_ok");
						if(ret > 0) {
							debug_print_buf_hex(D_BURST_DETAIL, rs_tab[r], RS_N, "Corrected block %d:\n", r);
							// count corrected octets, excluding intended erasures
							v->num_fec_corrections += ret - (RS_N - RS_K - num_fec_octets);
						}
					}
					if(r != v->num_blocks - 1)
						ret = bitstream_append_lsbfirst(v->bs, (uint8_t *)&rs_tab[r], RS_K, 8);
					else
						ret = bitstream_append_lsbfirst(v->bs, (uint8_t *)&rs_tab[r], v->last_block_len_octets, 8);
					if(ret < 0) {
						debug_print(D_BURST, "bitstream_append_lsbfirst failed\n");
						statsd_increment_per_channel(v->freq, "decoder.errors.bitstream");
						goto cleanup;
					}
				}
			}
			// bitstream_append_lsbfirst() reads whole bytes, but datalen usually isn't a multiple
			// of 8 due to bit stuffing, so we need to truncate the padding bits from the end of the bit stream.
			if(v->datalen < v->bs->end - v->bs->start) {
				debug_print(D_BURST, "Cut last %u bits from bitstream, bs->end was %u now is %u\n",
						v->bs->end - v->bs->start - v->datalen, v->bs->end, v->datalen);
				v->bs->end = v->datalen;
			}
			int ret;
			int frame_cnt = 0;
			while((ret = bitstream_copy_next_frame(v->bs, v->frame_bs)) >= 0) {
				if((v->frame_bs->end - v->frame_bs->start) % 8 != 0) {
					debug_print(D_BURST, "Frame %d: Bit stream error: does not end on a byte boundary\n", frame_cnt);
					statsd_increment_per_channel(v->freq, "decoder.errors.truncated_octets");
					goto cleanup;
				}
				debug_print(D_BURST, "Frame %d: Stream OK after unstuffing, length is %u octets\n",
						frame_cnt, (v->frame_bs->end - v->frame_bs->start) / 8);
				uint32_t frame_len_octets = (v->frame_bs->end - v->frame_bs->start) / 8;
				memset(data, 0, frame_len_octets * sizeof(uint8_t));
				if(bitstream_read_lsbfirst(v->frame_bs, data, frame_len_octets, 8) < 0) {
					debug_print(D_BURST, "Frame %d: bitstream_read_lsbfirst failed\n", frame_cnt);
					statsd_increment_per_channel(v->freq, "decoder.errors.bitstream");
					goto cleanup;
				}
				statsd_increment_per_channel(v->freq, "decoder.msg.good");
				decode_frame(v, frame_cnt, data, frame_len_octets);
				frame_cnt++;
				if(ret == 0) { // this was the last frame in this burst
					break;
				}
			}
			if(ret < 0) {
				statsd_increment_per_channel(v->freq, "decoder.errors.unstuff");
				goto cleanup;
			}
			statsd_timing_delta_per_channel(v->freq, "decoder.msg.processing_time", v->tstart);
cleanup:
			XFREE(data);
			XFREE(fec);
			v->decoder_state = DEC_IDLE;
			debug_print(D_BURST, "DEC_IDLE\n");
			return;
		case DEC_IDLE:
			return;
	}
}

static void output_queue_push(void *data, void *ctx) {
	ASSERT(data != NULL);
	ASSERT(ctx != NULL);
	CAST_PTR(output, output_instance_t *, data);
	CAST_PTR(qentry, output_qentry_t *, ctx);

	bool overflow = (g_async_queue_length(output->ctx->q) >= Config.output_queue_hwm);
	bool active = output->ctx->active;
	if(qentry->flags & OUT_FLAG_ORDERED_SHUTDOWN || (active && !overflow)) {
		output_qentry_t *copy = output_qentry_copy(qentry);
		g_async_queue_push(output->ctx->q, copy);
		debug_print(D_OUTPUT, "dispatched %s output %p\n", output->td->name, output);
	} else {
		if(overflow) {
			fprintf(stderr, "%s output queue overflow, throttling\n", output->td->name);
		} else if(!active) {
			debug_print(D_OUTPUT, "%s output %p is inactive, skipping\n", output->td->name, output);
		}
	}
}

static void shutdown_outputs(la_list *fmtr_list) {
	fmtr_instance_t *fmtr = NULL;
	for(la_list *p = fmtr_list; p != NULL; p = la_list_next(p)) {
		fmtr = (fmtr_instance_t *)(p->data);
		output_qentry_t qentry = {
			.msg = NULL,
			.metadata = NULL,
			.format = OFMT_UNKNOWN,
			.flags = OUT_FLAG_ORDERED_SHUTDOWN
		};
		la_list_foreach(fmtr->outputs, output_queue_push, &qentry);
	}
}

void *avlc_decoder_thread(void *arg) {
	ASSERT(arg != NULL);
	CAST_PTR(fmtr_list, la_list *, arg);
	avlc_frame_qentry_t *q = NULL;
	la_proto_node *root = NULL;
	uint32_t msg_type = 0;

	decoder_thread_active = true;
	la_reasm_ctx *reasm_ctx = la_reasm_ctx_new();
	enum {
		DEC_NOT_DONE,
		DEC_SUCCESS,
		DEC_FAILURE
	} decoding_status;
	while(1) {
		q = (avlc_frame_qentry_t *)g_async_queue_pop(avlc_decoder_queue);

		if(q->flags & OUT_FLAG_ORDERED_SHUTDOWN) {
			fprintf(stderr, "Shutting down decoder thread\n");
			shutdown_outputs(fmtr_list);
			decoder_thread_active = false;
			return NULL;
		}

		ASSERT(q->metadata != NULL);
		statsd_increment_per_channel(q->metadata->freq, "avlc.frames.processed");

		fmtr_instance_t *fmtr = NULL;
		decoding_status = DEC_NOT_DONE;
		for(la_list *p = fmtr_list; p != NULL; p = la_list_next(p)) {
			fmtr = (fmtr_instance_t *)(p->data);
			if(fmtr->intype == FMTR_INTYPE_DECODED_FRAME) {
				// Decode the frame unless we've done it before
				if(decoding_status == DEC_NOT_DONE) {
					msg_type = 0;
					root = avlc_parse(q, &msg_type, reasm_ctx);
					if(root != NULL) {
						decoding_status = DEC_SUCCESS;
					} else {
						decoding_status = DEC_FAILURE;
						la_proto_tree_destroy(root);
						root = NULL;
					}
				}
				if(decoding_status == DEC_SUCCESS) {
					if((msg_type & Config.msg_filter) == msg_type) {
						debug_print(D_OUTPUT, "msg_type: %x msg_filter: %x (accepted)\n", msg_type, Config.msg_filter);
						octet_string_t *serialized_msg = fmtr->td->format_decoded_msg(q->metadata, root);
						// First check if the formatter actually returned something.
						// A formatter might be suitable only for a particular message type. If this is the case.
						// it will return NULL for all messages it cannot handle.
						// An example is pp_acars which only deals with ACARS messages.
						if(serialized_msg != NULL) {
							output_qentry_t qentry = {
								.msg = serialized_msg,
								.metadata = q->metadata,
								.format = fmtr->td->output_format
							};
							la_list_foreach(fmtr->outputs, output_queue_push, &qentry);
							// output_queue_push makes a copy of serialized_msg, so it's safe to free it now
							octet_string_destroy(serialized_msg);
						}
					} else {
						debug_print(D_OUTPUT, "msg_type: %x msg_filter: %x (filtered out)\n", msg_type, Config.msg_filter);
					}
				}
			} else if(fmtr->intype == FMTR_INTYPE_RAW_FRAME) {
				octet_string_t *serialized_msg = fmtr->td->format_raw_msg(q->metadata, q->frame);
				if(serialized_msg != NULL) {
					output_qentry_t qentry = {
						.msg = serialized_msg,
						.metadata = q->metadata,
						.format = fmtr->td->output_format
					};
					la_list_foreach(fmtr->outputs, output_queue_push, &qentry);
					// output_queue_push makes a copy of serialized_msg, so it's safe to free it now
					octet_string_destroy(serialized_msg);
				}
			}
		}
		la_proto_tree_destroy(root);
		root = NULL;
		octet_string_destroy(q->frame);
		XFREE(q->metadata);
		XFREE(q);
	}
}

void avlc_decoder_init() {
	avlc_decoder_queue = g_async_queue_new();
}

void avlc_decoder_shutdown() {
	avlc_decoder_queue_push(NULL, NULL, OUT_FLAG_ORDERED_SHUTDOWN);
}
