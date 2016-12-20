#include <stdint.h>
#define MIN_ACARS_LEN 16	// including CRC and DEL

typedef struct {
	uint8_t mode;
	uint8_t reg[8];
	uint8_t ack;
	uint8_t label[3];
	uint8_t bid;
	uint8_t bs;
	uint8_t no[5];
	uint8_t fid[7];
	uint8_t txt[250];
} acars_msg_t;

acars_msg_t *parse_acars(uint8_t *buf, uint32_t len);
// vim: ts=4
