#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fec.h>
#include "rtlvdl2.h"

void *rs;

int rs_init() {
	rs = init_rs_char(8, 0x187, 120, 1, RS_N - RS_K, 0);
	return (rs ? 0 : -1);
}

int rs_verify(uint8_t *data, int fec_octets) {
	if(fec_octets == 0)
		return 0;
//	for(int i = 0; i < RS_N; i++) printf("%02x ", data[i]); printf("\n");
	debug_print_buf_hex(data, RS_N, "%s", "Input data:\n");
	int no_eras = RS_N - RS_K - fec_octets;
	int errlocs[RS_N];
	int ret;
	debug_print("no_eras=%d\n", no_eras);
	if(no_eras > 0) {
		memset(errlocs, 0, sizeof(errlocs));
		for(int i = RS_N - no_eras; i < RS_N; i++)
			errlocs[i] = 1;
//		for(int i = 0; i < RS_N; i++) printf("%d ", errlocs[i]); printf("\n");
		debug_print_buf_hex(errlocs, RS_N, "%s", "Errlocs:\n");
		ret = decode_rs_char(rs, data, errlocs, no_eras);
	} else {
		ret = decode_rs_char(rs, data, NULL, no_eras);
	}
	return ret;
}

void rs_encode(uint8_t *data, uint8_t *parity) {
	encode_rs_char(rs, data, parity);
}
