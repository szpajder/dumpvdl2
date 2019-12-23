/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
 *  Copyright (c) 2017-2019 Tomasz Lemiech <szpajder@gmail.com>
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
#include <glib.h>
#include <libacars/libacars.h>	// la_proto_node, la_proto_tree_destroy()
#include <libacars/reassembly.h>	// la_reasm_ctx, la_reasm_ctx_new()
#include "config.h"
#ifdef WITH_STATSD
#include <sys/time.h>
#endif
#include "dumpvdl2.h"
#include "avlc.h"		// avlc_frame_qentry_t
#include "acars.h"		// acars_output_pp

// Reasonable limits for transmission lengths in bits
// This is to avoid blocking the decoder in DEC_DATA for a long time
// in case when the transmission length field in the header gets
// decoded wrongly.
// This applies when header decoded OK without error corrections
#define MAX_FRAME_LENGTH 0x3FFF
// This applies when there were some bits corrected
#define MAX_FRAME_LENGTH_CORRECTED 0x1FFF

#define LFSR_IV 0x6959u

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
	debug_print("received: 0x%x syndrome: 0x%x error: 0x%x, decoded: 0x%x\n",
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
	if(fillwidth + offset > cols)					// fillwidth or offset too large
		return -2;
	if(len > rows * fillwidth)					// result won't fit
		return -3;
	if(rows > 1 && len - last_row_len < (rows - 1) * fillwidth)	// not enough data to fill requested width
		return -4;
	if(last_row_len == 0 && len / fillwidth < rows)			// not enough data to fill requested number of rows
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

GAsyncQueue *frame_queue = NULL;

static void enqueue_frame(vdl2_channel_t const * const v, int const frame_num, uint8_t *buf, size_t const len) {
	NEW(avlc_frame_qentry_t, qentry);
	qentry->buf = XCALLOC(len, sizeof(uint8_t));
	memcpy(qentry->buf, buf, len);
	qentry->len = len;
	qentry->freq = v->freq;
	qentry->frame_pwr = v->frame_pwr;
	qentry->mag_nf = v->mag_nf;
	qentry->ppm_error = v->ppm_error;
	qentry->burst_timestamp.tv_sec =  v->burst_timestamp.tv_sec;
	qentry->burst_timestamp.tv_usec =  v->burst_timestamp.tv_usec;
	if(extended_header) {
		qentry->datalen_octets = v->datalen_octets;
		qentry->synd_weight = synd_weight[v->syndrome];
		qentry->num_fec_corrections = v->num_fec_corrections;
		qentry->idx = frame_num;
	}
	g_async_queue_push(frame_queue, qentry);
}

void decode_vdl_frame(vdl2_channel_t *v) {
	switch(v->decoder_state) {
	case DEC_HEADER:
		v->lfsr = LFSR_IV;
		bitstream_descramble(v->bs, &v->lfsr);
		uint32_t header;
		if(bitstream_read_word_msbfirst(v->bs, &header, HEADER_LEN) < 0) {
			debug_print("Could not read header from bitstream\n");
			statsd_increment(v->freq, "decoder.errors.no_header");
			v->decoder_state = DEC_IDLE;
			return;
		}
// force bits of reserved symbol to 0 to improve chances of successful decode
		header &= ONES(TRLEN+HDRFECLEN);
		v->syndrome = decode_header(&header);
		if(v->syndrome == 0) {
			statsd_increment(v->freq, "decoder.crc.good");
		}
// sanity check - reserved symbol bits shall still be set to 0
		if((header & ONES(TRLEN+HDRFECLEN)) != header) {
			debug_print("Rejecting decoded header with non-zero reserved bits\n");
			statsd_increment(v->freq, "decoder.crc.bad");
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
			debug_print("v->datalen=%u v->syndrome=%u - frame rejected\n", v->datalen, v->syndrome);
			statsd_increment(v->freq, "decoder.errors.too_long");
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

		debug_print("Data length: %u (0x%x) bits (%u octets), num_blocks=%u, last_block_len_octets=%u fec_octets=%u\n",
			v->datalen, v->datalen, v->datalen_octets, v->num_blocks, v->last_block_len_octets, v->fec_octets);

		if(v->fec_octets == 0) {
			debug_print("fec_octets is 0 which means the frame is unreasonably short\n");
			statsd_increment(v->freq, "decoder.errors.no_fec");
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
			debug_print("Frame data truncated\n");
			statsd_increment(v->freq, "decoder.errors.data_truncated");
			goto cleanup;
		}
		if(bitstream_read_lsbfirst(v->bs, fec, v->fec_octets, 8) < 0) {
			debug_print("FEC data truncated\n");
			statsd_increment(v->freq, "decoder.errors.fec_truncated");
			goto cleanup;
		}
		debug_print_buf_hex(data, v->datalen_octets, "Data:\n");
		debug_print_buf_hex(fec, v->fec_octets, "FEC:\n") ;
		{
			uint8_t rs_tab[v->num_blocks][RS_N];
			memset(rs_tab, 0, sizeof(uint8_t[v->num_blocks][RS_N]));
			int ret;
			if((ret = deinterleave(data, v->datalen_octets, v->num_blocks, RS_N, rs_tab, RS_K, 0)) < 0) {
				debug_print("Deinterleaver failed with error %d\n", ret);
				statsd_increment(v->freq, "decoder.errors.deinterleave_data");
				goto cleanup;
			}

// if last block is < 3 bytes long, no FEC is done on it, so we should not write FEC bytes into the last row
			uint32_t fec_rows = v->num_blocks;
			if(get_fec_octetcount(v->last_block_len_octets) == 0)
				fec_rows--;

			if((ret = deinterleave(fec, v->fec_octets, fec_rows, RS_N, rs_tab, RS_N - RS_K, RS_K)) < 0) {
				debug_print("Deinterleaver failed with error %d\n", ret);
				statsd_increment(v->freq, "decoder.errors.deinterleave_fec");
				goto cleanup;
			}
#ifdef DEBUG
			debug_print("Deinterleaved blocks:\n");
			for(uint32_t r = 0; r < v->num_blocks; r++) {
				debug_print_buf_hex(rs_tab[r], RS_N, "Block %d:\n", r);
			}
#endif
			bitstream_reset(v->bs);
			for(uint32_t r = 0; r < v->num_blocks; r++) {
				statsd_increment(v->freq, "decoder.blocks.processed");
				int num_fec_octets = RS_N - RS_K;	// full block
				if(r == v->num_blocks - 1) {		// final, partial block
					num_fec_octets = get_fec_octetcount(v->last_block_len_octets);
				}
				ret = rs_verify((uint8_t *)&rs_tab[r], num_fec_octets);
				debug_print("Block %d FEC: %d\n", r, ret);
				if(ret < 0) {
					debug_print("FEC check failed\n");
					statsd_increment(v->freq, "decoder.errors.fec_bad");
					goto cleanup;
				} else {
					statsd_increment(v->freq, "decoder.blocks.fec_ok");
					if(ret > 0) {
						debug_print_buf_hex(rs_tab[r], RS_N, "Corrected block %d:\n", r);
// count corrected octets, excluding intended erasures
						v->num_fec_corrections += ret - (RS_N - RS_K - num_fec_octets);
					}
				}
				if(r != v->num_blocks - 1)
					ret = bitstream_append_lsbfirst(v->bs, (uint8_t *)&rs_tab[r], RS_K, 8);
				else
					ret = bitstream_append_lsbfirst(v->bs, (uint8_t *)&rs_tab[r], v->last_block_len_octets, 8);
				if(ret < 0) {
					debug_print("bitstream_append_lsbfirst failed\n");
					statsd_increment(v->freq, "decoder.errors.bitstream");
					goto cleanup;
				}
			}
		}
// bitstream_append_lsbfirst() reads whole bytes, but datalen usually isn't a multiple of 8 due to bit stuffing.
// So we need to truncate the padding bits from the end of the bit stream.
		if(v->datalen < v->bs->end - v->bs->start) {
			debug_print("Cut last %u bits from bitstream, bs->end was %u now is %u\n",
				v->bs->end - v->bs->start - v->datalen, v->bs->end, v->datalen);
			v->bs->end = v->datalen;
		}
		int ret;
		int frame_cnt = 0;
		while((ret = bitstream_copy_next_frame(v->bs, v->frame_bs)) >= 0) {
			if((v->frame_bs->end - v->frame_bs->start) % 8 != 0) {
				debug_print("Frame %d: Bit stream error: does not end on a byte boundary\n", frame_cnt);
				statsd_increment(v->freq, "decoder.errors.truncated_octets");
				goto cleanup;
			}
			debug_print("Frame %d: Stream OK after unstuffing, length is %u octets\n",
				frame_cnt, (v->frame_bs->end - v->frame_bs->start) / 8);
			uint32_t frame_len_octets = (v->frame_bs->end - v->frame_bs->start) / 8;
			memset(data, 0, frame_len_octets * sizeof(uint8_t));
			if(bitstream_read_lsbfirst(v->frame_bs, data, frame_len_octets, 8) < 0) {
				debug_print("Frame %d: bitstream_read_lsbfirst failed\n", frame_cnt);
				statsd_increment(v->freq, "decoder.errors.bitstream");
				goto cleanup;
			}
			statsd_increment(v->freq, "decoder.msg.good");
			enqueue_frame(v, frame_cnt, data, frame_len_octets);
			frame_cnt++;
			if(ret == 0) break;	// this was the last frame in this burst
		}
		if(ret < 0) {
			statsd_increment(v->freq, "decoder.errors.unstuff");
			goto cleanup;
		}
		statsd_timing_delta(v->freq, "decoder.msg.processing_time", &v->tstart);
cleanup:
		XFREE(data);
		XFREE(fec);
		v->decoder_state = DEC_IDLE;
		debug_print("DEC_IDLE\n");
		return;
	case DEC_IDLE:
		return;
	}
}

void *avlc_decoder_thread(void *arg) {
	UNUSED(arg);
	avlc_frame_qentry_t *q = NULL;
	la_proto_node *root = NULL;
	uint32_t msg_type = 0;

	frame_queue = g_async_queue_new();
	la_reasm_ctx *reasm_ctx = la_reasm_ctx_new();
	while(1) {
		q = (avlc_frame_qentry_t *)g_async_queue_pop(frame_queue);
		statsd_increment(q->freq, "avlc.frames.processed");
		msg_type = 0;
		root = avlc_parse(q, &msg_type, reasm_ctx);
		if(root == NULL) {
			goto cleanup;
		}
		if((msg_type & msg_filter) == msg_type) {
			debug_print("msg_type: %x msg_filter: %x (accepted)\n", msg_type, msg_filter);
			output_proto_tree(root);
		} else {
			debug_print("msg_type: %x msg_filter: %x (filtered out)\n", msg_type, msg_filter);
		}
		acars_output_pp(root);
cleanup:
		la_proto_tree_destroy(root);
		root = NULL;
		XFREE(q->buf);
		XFREE(q);
	}
}
