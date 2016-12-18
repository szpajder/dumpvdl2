#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include "rtlvdl2.h"

const uint32_t H[CRCLEN] = { 0x00FFF, 0x3F0FF, 0xC730F, 0xDB533, 0x69E55 };
const uint8_t preamble_bits[PREAMBLE_SYMS*BPS] = {
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
//		debug_print("row %d: 0x%x sum=%d\n", i, row, parity(row));
		r |= (parity(row)) << (CRCLEN - 1 - i);
	}
	debug_print("crc: read 0x%x calculated 0x%x\n", check, r);
	return (r == check ? 1 : 0);
}

uint8_t *skip_preamble(bitstream_t *bs) {
	if(bs == NULL) return NULL;
	uint8_t *start = (uint8_t *)memmem(bs->buf + bs->start, bs->end - bs->start, preamble_bits, BPS*PREAMBLE_SYMS);
	if(start != NULL) {
		bs->start = start - bs->buf;
		debug_print("Preamble found at %u\n", bs->start);
		bs->start += BPS*PREAMBLE_SYMS;
		debug_print("Now at %u\n", bs->start);
	}
	return start;
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
		if(skip_preamble(v->bs) == NULL) {
			debug_print("%s", "No preamble found\n");
			v->decoder_state = DEC_IDLE;
			return;
		}
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
			v->decoder_state = DEC_IDLE;
			return;
		}
		uint32_t crc = header & ONES(CRCLEN);
		header >>= CRCLEN;
		if(!check_crc(header, crc)) {
			debug_print("%s", "CRC check failed\n");
			v->decoder_state = DEC_IDLE;
			return;
		}
		v->datalen = reverse(header & ONES(TRLEN), TRLEN);
		v->datalen_octets = v->datalen / 8;
		if(v->datalen % 8 != 0)
			v->datalen_octets++;
		v->num_blocks = v->datalen_octets / RS_K;
		v->fec_octets = v->num_blocks * 6;
		v->last_block_len_octets = v->datalen_octets % RS_K;
		if(v->last_block_len_octets != 0)
			v->num_blocks++;

		v->fec_octets += get_fec_octetcount(v->last_block_len_octets);

		debug_print("Data length: %u (0x%x) bits (%u octets), num_blocks=%u, last_block_len_octets=%u fec_octets=%u\n",
			v->datalen, v->datalen, v->datalen_octets, v->num_blocks, v->last_block_len_octets, v->fec_octets);

		if(v->fec_octets == 0) {
			debug_print("%s", "fec_octets is 0 which means the frame is unreasonably short\n");
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
			goto cleanup;
		}
		uint8_t *fec = calloc(v->fec_octets, sizeof(uint8_t));
		if(fec == NULL) {
			fprintf(stderr, "%s", "calloc(fec) failed\n");
			_exit(1);
		}
		if(bitstream_read_lsbfirst(v->bs, fec, v->fec_octets, 8) < 0) {		// no padding allowed
			debug_print("%s", "FEC data truncated\n");
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
				goto cleanup;
			}
// FIXME: if last block is < 3 bytes, we should fill num_blocks - 1 rows
			if((ret = deinterleave(fec, v->fec_octets, v->num_blocks, RS_N, rs_tab, RS_N - RS_K, RS_K)) < 0) {
				debug_print("Deinterleaver failed with error %d\n", ret);
				goto cleanup;
			}
			debug_print("%s", "Deinterleaved blocks:\n");
			for(int r = 0; r < v->num_blocks; r++) {
				debug_print_buf_hex(rs_tab[r], RS_N, "Block %d:\n", r);
			}
			bitstream_reset(v->bs);
			for(int r = 0; r < v->num_blocks; r++) {
				if(r != v->num_blocks - 1)		// full block
					ret = rs_verify((uint8_t *)&rs_tab[r], RS_N - RS_K);
				else							// last, partial block
					ret = rs_verify((uint8_t *)&rs_tab[r], get_fec_octetcount(v->last_block_len_octets));
				debug_print("Block %d FEC: %d\n", r, ret);
				if(ret < 0) {
					debug_print("%s", "FEC check failed\n");
//					goto cleanup;
				} else if(ret > 0) {
					debug_print_buf_hex(rs_tab[r], RS_N, "Corrected block %d:\n", r);
				}
				uint8_t rs_parity[RS_N-RS_K];
				memset(rs_parity, 0, sizeof(parity));
				rs_encode((uint8_t *)&rs_tab[r], rs_parity);
				debug_print_buf_hex(rs_parity, sizeof(rs_parity), "%s", "Calculated FEC:\n");
				
				if(r != v->num_blocks - 1)
					ret = bitstream_append_lsbfirst(v->bs, (uint8_t *)&rs_tab[r], RS_N - RS_K, 8);
				else
					ret = bitstream_append_lsbfirst(v->bs, (uint8_t *)&rs_tab[r], v->last_block_len_octets, 8);
				if(ret < 0) {
					debug_print("%s", "bitstream_append_lsbfirst failed\n");
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
			goto cleanup;
		}
		if((v->bs->end - v->bs->start) % 8 != 0) {
			debug_print("%s", "Bit stream error: does not end on a byte boundary\n");
			goto cleanup;
		}
		debug_print("stream OK after unstuffing, datalen_octets was %u now is %u\n", v->datalen_octets, ((v->bs->end - v->bs->start) / 8));
		v->datalen_octets = (v->bs->end - v->bs->start) / 8;

		memset(data, 0, v->datalen_octets * sizeof(uint8_t));		// FIXME: is this really needed?
		if(bitstream_read_lsbfirst(v->bs, data, v->datalen_octets, 8) < 0) {
			debug_print("%s", "bitstream_read_lsbfirst failed\n");
			goto cleanup;
		}
		parse_avlc_frames(data, v->datalen_octets);
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
// vim: ts=4
