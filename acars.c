#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "rtlvdl2.h"
#include "acars.h"

//#define STX 0x02
#define ETX 0x83
#define ETB 0x97
#define DEL 0x7f

/* ACARS message decoder
 * Based on acarsdec by Thierry Leconte
 */
acars_msg_t *parse_acars(uint8_t *buf, uint32_t len) {
	static acars_msg_t *msg = NULL;
	int i;

	if(msg == NULL) {
		msg = calloc(1, sizeof(acars_msg_t));
		if(msg == NULL) {
			fprintf(stderr, "parse_acars: calloc failed\n");
			_exit(1);
		}
	}
	if(len < MIN_ACARS_LEN) {
		debug_print("too short: %u < %u\n", len, MIN_ACARS_LEN);
		return NULL;
	}

	if(buf[len-1] != DEL) {
		debug_print("%02x: no DEL byte at end\n", buf[len-1]);
		return NULL;
	}
	if(buf[len-4] != ETX && buf[len-4] != ETB) {
		debug_print("%02x: no ETX/ETB byte at end\n", buf[len-4]);
		return NULL;
	}
	len -= 4;
	memset(msg, 0, sizeof(acars_msg_t));
// FIXME: check CRC and parity

	for(i = 0; i < len; i++)
		buf[i] &= 0x7f;

	uint32_t k = 0;
	msg->mode = buf[k++];

	for (i = 0; i < 7; i++, k++) {
		msg->reg[i] = buf[k];
	}
	msg->reg[7] = '\0';

	/* ACK/NAK */
	msg->ack = buf[k++];
	if (msg->ack == 0x15)
		msg->ack = '!';

	msg->label[0] = buf[k++];
	msg->label[1] = buf[k++];
	if (msg->label[1] == 0x7f)
		msg->label[1] = 'd';
	msg->label[2] = '\0';

	msg->bid = buf[k++];
	if (msg->bid == 0)
		msg->bid = ' ';

	/* txt start  */
	msg->bs = buf[k++];

	msg->no[0] = '\0';
	msg->fid[0] = '\0';
	msg->txt[0] = '\0';

//	if (msg->bs == 0x03 || msg->mode > 'Z' || msg->bid > '9') {
//#ifdef WITH_STATSD
//		increment(blk->chn, "msg->ground2air");
//#endif
//		if(airflt)
//			return;
//	}

	if(k >= len) {		// empty txt
		msg->txt[0] = '\0';
		return msg;
	}

	if (msg->bs != 0x03) {
//#ifdef WITH_STATSD
//		increment(blk->chn, "msg->air2ground");
//#endif
		if (msg->mode <= 'Z' && msg->bid <= '9') {
			/* message no */
			for (i = 0; i < 4 && k < len; i++, k++) {
				msg->no[i] = buf[k];
			}
			msg->no[i] = '\0';

			/* Flight id */
			for (i = 0; i < 6 && k < len; i++, k++) {
				msg->fid[i] = buf[k];
			}
			msg->fid[i] = '\0';
		}

		/* Message txt */
		len -= k;
		if(len > ACARSMSG_BUFSIZE) {
			debug_print("message truncated to buffer size (%u > %u)", len, ACARSMSG_BUFSIZE);
			len = ACARSMSG_BUFSIZE - 1;		// leave space for terminating '\0'
		}
		if(len > 0)
			memcpy(msg->txt, buf + k, len);
		msg->txt[len] = '\0';
	}
	/* txt end */
	return msg;
}
// vim: ts=6
