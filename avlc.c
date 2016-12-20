#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "rtlvdl2.h"
#include "avlc.h"

void print_header_data(avlc_addr_t *s, avlc_addr_t *d, lcf_t lcf) {
	printf("%06X (%s, %s) -> %06X (%s): %s, CF: 0x%02x\n",
		s->a_addr.addr,
		addrtype_descr[s->a_addr.type],
		status_ag_descr[d->a_addr.status],	// A/G
		d->a_addr.addr,
		addrtype_descr[d->a_addr.type],
		status_cr_descr[s->a_addr.status],	// C/R
		lcf.val
	);
	if(IS_S(lcf)) {
		printf("  S: sfunc=0x%x (%s) P/F=%x rseq=0x%x\n", lcf.S.sfunc, S_cmd[lcf.S.sfunc], lcf.S.pf, lcf.S.recv_seq);
	} else if(IS_U(lcf)) {
		printf("  U: mfunc=%02x (%s) P/F=%x\n", U_MFUNC(lcf), U_cmd[U_MFUNC(lcf)], U_PF(lcf));
	} else {	// IS_U == true
		printf("  I: sseq=0x%x rseq=0x%x poll=%x\n", lcf.I.send_seq, lcf.I.recv_seq, lcf.I.poll);
	}
}
		
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
	avlc_addr_t src, dst;
	if(parse_dlc_addr(ptr, &dst, 0) < 0) return;
	ptr += 4; len -= 4;
	if(parse_dlc_addr(ptr, &src, 1) < 0) return;
	ptr += 4; len -= 4;
	lcf_t lcf;
	lcf.val = *ptr++;
	len--;
	print_header_data(&src, &dst, lcf);
	printf("     ");
	for(int i = 0; i < len; i++)
		printf("%02x ", ptr[i]);
	printf("\n\n");
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
