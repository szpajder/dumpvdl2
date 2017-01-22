#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#if USE_STATSD
#include <sys/time.h>
#endif
#include "dumpvdl2.h"

static const uint32_t H[CRCLEN] = { 0x00FFF, 0x3F0FF, 0xC730F, 0xDB533, 0x69E55 };
static const uint8_t preamble_bits[PREAMBLE_SYMS*BPS] = {
	0,0,0,
	0,1,0,
	0,1,1,
	1,1,0,
	0,0,0,
	0,0,1,
	1,0,1,
	1,1,0,
	0,0,1,
	1,0,0,
	0,1,1,
	1,1,1,
	1,0,1,
	1,1,1,
	1,0,0,
	0,1,0
};

// FIXME: precompute?
uint32_t parity(uint32_t v) {
	uint32_t parity = 0;
	while (v) {
	  parity = !parity;
	  v = v & (v - 1);
	}
	return parity;
}

uint32_t check_crc(uint32_t v, uint32_t check) {
	uint32_t r = 0, row = 0;
	int i;
	for(i = 0; i < CRCLEN; i++) {
		row = v & H[i];
		r |= (parity(row)) << (CRCLEN - 1 - i);
	}
	debug_print("crc: read 0x%x calculated 0x%x\n", check, r);
	return (r == check ? 1 : 0);
}

static uint8_t *soft_preamble_search(bitstream_t *bs) {
	if(bs == NULL) return NULL;
	uint32_t pr_len = PREAMBLE_LEN;
	uint32_t bs_len = bs->end - bs->start;
	if(bs_len < pr_len) {
		debug_print("%s", "haystack too short\n");
		return NULL;
	}
	uint32_t min_distance, distance, best_match;
	int i,j,k;
	min_distance = pr_len;
	for(i = 0; i <= bs_len - pr_len; i++) {
		distance = 0;
		for(j = bs->start + i, k = 0; k < pr_len; j++, k++)
			distance += bs->buf[j] ^ preamble_bits[k];
		if(distance < min_distance) {
			best_match = i;
			min_distance = distance;
			if(distance == 0) break;	// exact match
		}
	}
	if(min_distance > MAX_PREAMBLE_ERRORS) {
		debug_print("Preamble not found (min_distance %u > %u)\n", min_distance, MAX_PREAMBLE_ERRORS);
		return NULL;
	}
	debug_print("Preamble found at %u (distance %u)\n", best_match, min_distance);
	bs->start = best_match + pr_len;
	debug_print("Now at %u\n", bs->start);
	return bs->buf + best_match;
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

void decode_vdl_frame(vdl2_state_t *v) {
	switch(v->decoder_state) {
	case DEC_PREAMBLE:
#if USE_STATSD
		gettimeofday(&v->tstart, NULL);
#endif
		if(soft_preamble_search(v->bs) == NULL) {
			statsd_increment("decoder.errors.no_preamble");
			v->decoder_state = DEC_IDLE;
			return;
		}
		statsd_increment("decoder.preambles.good");
		v->decoder_state = DEC_HEADER;
		v->requested_bits = HEADER_LEN;
		debug_print("DEC_HEADER, requesting %u bits\n", v->requested_bits);
		return;
	case DEC_HEADER:
		v->lfsr = LFSR_IV;
		bitstream_descramble(v->bs, &v->lfsr);
		uint32_t header;
		if(bitstream_read_word_msbfirst(v->bs, &header, HEADER_LEN) < 0) {
			debug_print("%s", "Could not read header from bitstream\n");
			statsd_increment("decoder.errors.no_header");
			v->decoder_state = DEC_IDLE;
			return;
		}
		uint32_t crc = header & ONES(CRCLEN);
		header >>= CRCLEN;
		if(!check_crc(header, crc)) {
			debug_print("%s", "CRC check failed\n");
			statsd_increment("decoder.errors.crc_bad");
			v->decoder_state = DEC_IDLE;
			return;
		}
		statsd_increment("decoder.crc.good");
		v->datalen = reverse(header & ONES(TRLEN), TRLEN);
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
			debug_print("%s", "fec_octets is 0 which means the frame is unreasonably short\n");
			statsd_increment("decoder.errors.no_fec");
			v->decoder_state = DEC_IDLE;
			return;
		}
		v->requested_bits = 8 * (v->datalen_octets + v->fec_octets);
		v->decoder_state = DEC_DATA;
		return;
	case DEC_DATA:
		bitstream_descramble(v->bs, &v->lfsr);
		uint8_t *data = (uint8_t *)calloc(v->datalen_octets, sizeof(uint8_t));
		if(data == NULL) {
			fprintf(stderr, "%s", "calloc(data) failed\n");
			_exit(1);
		}
		if(bitstream_read_lsbfirst(v->bs, data, v->datalen_octets, 8) < 0) {
			debug_print("%s", "Frame data truncated\n");
			statsd_increment("decoder.errors.data_truncated");
			goto cleanup;
		}
		uint8_t *fec = calloc(v->fec_octets, sizeof(uint8_t));
		if(fec == NULL) {
			fprintf(stderr, "%s", "calloc(fec) failed\n");
			_exit(1);
		}
		if(bitstream_read_lsbfirst(v->bs, fec, v->fec_octets, 8) < 0) {
			debug_print("%s", "FEC data truncated\n");
			statsd_increment("decoder.errors.fec_truncated");
			goto cleanup;
		}
		debug_print_buf_hex(data, v->datalen_octets, "%s", "Data:\n");
		debug_print_buf_hex(fec, v->fec_octets, "%s", "FEC:\n") ;
		{
			uint8_t rs_tab[v->num_blocks][RS_N];
			memset(rs_tab, 0, sizeof(uint8_t[v->num_blocks][RS_N]));
			int ret;
			if((ret = deinterleave(data, v->datalen_octets, v->num_blocks, RS_N, rs_tab, RS_K, 0)) < 0) {
				debug_print("Deinterleaver failed with error %d\n", ret);
				statsd_increment("decoder.errors.deinterleave_data");
				goto cleanup;
			}

// if last block is < 3 bytes long, no FEC is done on it, so we should not write FEC bytes into the last row
			uint32_t fec_rows = v->num_blocks;
			if(get_fec_octetcount(v->last_block_len_octets) == 0)
				fec_rows--;

			if((ret = deinterleave(fec, v->fec_octets, fec_rows, RS_N, rs_tab, RS_N - RS_K, RS_K)) < 0) {
				debug_print("Deinterleaver failed with error %d\n", ret);
				statsd_increment("decoder.errors.deinterleave_fec");
				goto cleanup;
			}
			debug_print("%s", "Deinterleaved blocks:\n");
			for(int r = 0; r < v->num_blocks; r++) {
				debug_print_buf_hex(rs_tab[r], RS_N, "Block %d:\n", r);
			}
			bitstream_reset(v->bs);
			for(int r = 0; r < v->num_blocks; r++) {
				statsd_increment("decoder.blocks.processed");
				if(r != v->num_blocks - 1)		// full block
					ret = rs_verify((uint8_t *)&rs_tab[r], RS_N - RS_K);
				else							// last, partial block
					ret = rs_verify((uint8_t *)&rs_tab[r], get_fec_octetcount(v->last_block_len_octets));
				debug_print("Block %d FEC: %d\n", r, ret);
				if(ret < 0) {
					debug_print("%s", "FEC check failed\n");
					statsd_increment("decoder.errors.fec_bad");
					goto cleanup;
				} else {
					statsd_increment("decoder.blocks.fec_ok");
					if(ret > 0)
						debug_print_buf_hex(rs_tab[r], RS_N, "Corrected block %d:\n", r);
				}
				
				if(r != v->num_blocks - 1)
					ret = bitstream_append_lsbfirst(v->bs, (uint8_t *)&rs_tab[r], RS_K, 8);
				else
					ret = bitstream_append_lsbfirst(v->bs, (uint8_t *)&rs_tab[r], v->last_block_len_octets, 8);
				if(ret < 0) {
					debug_print("%s", "bitstream_append_lsbfirst failed\n");
					statsd_increment("decoder.errors.bitstream");
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
		if(bitstream_hdlc_unstuff(v->bs) < 0) {
			debug_print("%s", "Invalid bit sequence in the stream\n");
			statsd_increment("decoder.errors.unstuff");
			goto cleanup;
		}
		if((v->bs->end - v->bs->start) % 8 != 0) {
			debug_print("%s", "Bit stream error: does not end on a byte boundary\n");
			statsd_increment("decoder.errors.truncated_octets");
			goto cleanup;
		}
		debug_print("stream OK after unstuffing, datalen_octets was %u now is %u\n", v->datalen_octets, ((v->bs->end - v->bs->start) / 8));
		v->datalen_octets = (v->bs->end - v->bs->start) / 8;

		memset(data, 0, v->datalen_octets * sizeof(uint8_t));
		if(bitstream_read_lsbfirst(v->bs, data, v->datalen_octets, 8) < 0) {
			debug_print("%s", "bitstream_read_lsbfirst failed\n");
			statsd_increment("decoder.errors.bitstream");
			goto cleanup;
		}
		statsd_increment("decoder.msg.good");
		parse_avlc_frames(v, data, v->datalen_octets);
		statsd_timing_delta("decoder.msg.processing_time", &v->tstart);
cleanup:
		if(data) free(data);
		if(fec) free(fec);
		v->decoder_state = DEC_IDLE;
		debug_print("%s", "DEC_IDLE\n");
		return;
	case DEC_IDLE:
		return;
	}
}
