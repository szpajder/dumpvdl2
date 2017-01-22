#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fec.h>
#include "dumpvdl2.h"

void *rs;

int rs_init() {
	rs = init_rs_char(8, 0x187, 120, 1, RS_N - RS_K, 0);
	return (rs ? 0 : -1);
}

int rs_verify(uint8_t *data, int fec_octets) {
	if(fec_octets == 0)
		return 0;
	debug_print_buf_hex(data, RS_N, "%s", "Input data:\n");
	int erasure_cnt = RS_N - RS_K - fec_octets;
	int ret;
	debug_print("erasure_cnt=%d\n", erasure_cnt);
	if(erasure_cnt > 0) {
		int erasures[erasure_cnt];
		for(int i = 0; i < erasure_cnt; i++)
			erasures[i] = RS_K + fec_octets + i;
		debug_print_buf_hex(erasures, erasure_cnt, "%s", "Erasures:\n");
		ret = decode_rs_char(rs, data, erasures, erasure_cnt);
	} else {
		ret = decode_rs_char(rs, data, NULL, erasure_cnt);
	}
	return ret;
}
