/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
 *  Copyright (c) 2017 Tomasz Lemiech <szpajder@gmail.com>
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
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "dumpvdl2.h"
#include "acars.h"
#include "adsc.h"

#define ETX 0x83
#define ETB 0x97
#define DEL 0x7f

static int try_fans1a_adsc(acars_msg_t *msg, uint32_t *msg_type) {
	uint8_t *buf = NULL;
	int ret = -1;

	char *s = strstr(msg->txt, ".ADS");
	if(s == NULL) {
		debug_print("%s", "not an ADS message\n");
		goto end;
	}
	s += 4;
	if(strlen(s) < 7 || memcmp(s, msg->reg, 7)) {
		debug_print("%s", "regnr not found\n");
		goto end;
	}
	s += 7;
	int64_t buflen = slurp_hexstring(s, &buf);
	if(buflen < 0) {
		goto end;
	}
	msg->data = adsc_parse_msg(buf, (size_t)buflen, msg_type);
	if(msg->data != NULL) {
		msg->application = ACARS_APP_FANS1A_ADSC;
		ret = 0;
	}
end:
	free(buf);
	return ret;
}

static void try_acars_apps(acars_msg_t *msg, uint32_t *msg_type) {
	if(	!memcmp(msg->label, "A6", 2) ||
		!memcmp(msg->label, "B6", 2) ||
		!memcmp(msg->label, "H1", 2)) {
			if(try_fans1a_adsc(msg, msg_type) == 0)
				return;
	}
}

/*
 * ACARS message decoder
 * Based on acarsdec by Thierry Leconte
 */
acars_msg_t *parse_acars(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	static acars_msg_t *msg = NULL;
	int i;

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
	if(msg == NULL)
		msg = XCALLOC(1, sizeof(acars_msg_t));
	else
		memset(msg, 0, sizeof(acars_msg_t));

	// safe default
	*msg_type |= MSGFLT_ACARS_NODATA;
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
	msg->application = ACARS_APP_NONE;

	if(k >= len) {		// empty txt
		msg->txt[0] = '\0';
		return msg;
	}

	if (msg->bs != 0x03) {
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
		if(len > 0) {
			memcpy(msg->txt, buf + k, len);
			*msg_type |= MSGFLT_ACARS_DATA;
			*msg_type &= ~MSGFLT_ACARS_NODATA;
		}
		msg->txt[len] = '\0';
		if(len > 0) {
			try_acars_apps(msg, msg_type);
		}
	}
	/* txt end */
	return msg;
}

void output_acars_pp(const acars_msg_t *msg) {
	char pkt[ACARSMSG_BUFSIZE+32];
	char txt[ACARSMSG_BUFSIZE];

	strcpy(txt, msg->txt);
	for(char *ptr = txt; *ptr != 0; ptr++)
		if (*ptr == '\n' || *ptr == '\r')
			*ptr = ' ';

	sprintf(pkt, "AC%1c %7s %1c %2s %1c %4s %6s %s",
		msg->mode, msg->reg, msg->ack, msg->label, msg->bid, msg->no, msg->fid, txt);

	if(write(pp_sockfd, pkt, strlen(pkt)) < 0)
		debug_print("write(pp_sockfd) error: %s", strerror(errno));
}

void output_acars(const acars_msg_t *msg) {
	fprintf(outf, "ACARS:\n");
	if(msg->mode < 0x5d)
		fprintf(outf, "Reg: %s Flight: %s\n", msg->reg, msg->fid);
	fprintf(outf, "Mode: %1c Label: %s Blk id: %c Ack: %c Msg no.: %s\n",
		msg->mode, msg->label, msg->bid, msg->ack, msg->no);
	fprintf(outf, "Message:\n%s\n", msg->txt);
	switch(msg->application) {
	case ACARS_APP_FANS1A_ADSC:
		adsc_output_msg(msg->data);
		break;
	case ACARS_APP_NONE:
	default:
		break;
	}
	if(pp_sockfd > 0)
		output_acars_pp(msg);
}
