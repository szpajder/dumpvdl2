#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "rtlvdl2.h"
#include "avlc.h"
#include "acars.h"

/* Link layer address parsing routine
 * buf - data buffer pointer
 * final - shall be set to 1 if this is the source address (ie. the final field
 * in the address part of the frame), 0 otherwise
 */
int parse_dlc_addr(uint8_t *buf, avlc_addr_t *a, uint8_t final) {
	debug_print("%02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3]);
/* Validate address field:
 * - LSBs of buf[0]-buf[2] must be 0
 * - LSB of buf[3] must be equal to final
 */
	uint32_t result = (uint32_t)(final & 0x1) << BSHIFT;
	if((*((uint32_t *)buf) & 0x01010101) != result) {
		debug_print("%s", "parse_dlc_addr: invalid address field\n");
		return -1;
	}
	a->val = reverse((buf[0] >> 1) | (buf[1] << 6) | (buf[2] << 13) | ((buf[3] & 0xfe) << 20), 28) & ONES(28);
//	debug_print("   Raw:    addr : 0x%6x type=0x%x A/G=%x\n", a->val & ONES(24), (a->val & 0x7000000) >> 24, (a->val & 0x8000000) >> 27);
	debug_print("Struct: addr : 0x%6x type=0x%x A/G=%x\n", a->a_addr.addr, a->a_addr.type, a->a_addr.status);
	return 0;
}

void parse_avlc(uint8_t *buf, uint32_t len) {
	debug_print_buf_hex(buf, len, "%s", "Frame data:\n");
// FCS check
	len -= 2;
	uint16_t read_fcs = ((uint16_t)buf[len+1] << 8) | buf[len];
	uint16_t fcs = crc16_ccitt(buf, len);
	debug_print("Read FCS : %04x\n", read_fcs);
	debug_print("Check FCS: %04x\n", fcs);
	if(read_fcs == fcs) {
		debug_print("%s", "FCS check OK\n");
	} else {
		debug_print("%s", "FCS check failed\n");
		return;
	}
	uint8_t *ptr = buf;
	avlc_frame_t frame;
	frame.t = time(NULL);
	if(parse_dlc_addr(ptr, &frame.dst, 0) < 0) return;
	ptr += 4; len -= 4;
	if(parse_dlc_addr(ptr, &frame.src, 1) < 0) return;
	ptr += 4; len -= 4;
	frame.lcf.val = *ptr++;
	len--;
	if(len > 3 && ptr[0] == 0xff && ptr[1] == 0xff && ptr[2] == 0x01) {
		frame.proto = PROTO_ACARS;
		frame.data = parse_acars(ptr + 3, len - 3);
	} else {
		frame.proto = PROTO_UNKNOWN;
		frame.data = ptr;
		frame.datalen = len;
	}
	output_avlc(&frame);
}

void parse_avlc_frames(uint8_t *buf, uint32_t len) {
	if(buf[0] != AVLC_FLAG) {
		debug_print("%s", "No AVLC frame delimiter at the start\n");
		return;
	}
	uint32_t fcnt = 0, goodfcnt = 0;
	uint8_t *frame_start = buf + 1;
	uint8_t *frame_end;
	uint8_t *buf_end = buf + len;
	uint32_t flen;
	while(frame_start < buf_end - 1) {
		if((frame_end = memchr(frame_start, AVLC_FLAG, buf_end - frame_start)) == NULL) {
			debug_print("Frame %u: truncated\n", fcnt);
			return;
		}
		flen = frame_end - frame_start;
		if(flen < MIN_AVLC_LEN) {
			debug_print("Frame %u: too short (len=%u required=%d)\n", fcnt, flen, MIN_AVLC_LEN);
			goto next;
		}
		debug_print("Frame %u: len=%u\n", fcnt, flen);
		goodfcnt++;
		parse_avlc(frame_start, flen);
next:
		frame_start = frame_end + 1;
		fcnt++;
	}
	debug_print("%u/%u good frames parsed\n", goodfcnt, fcnt);
}
// vim: ts=4
