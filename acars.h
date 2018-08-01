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
#include <stdint.h>
#define MIN_ACARS_LEN		16	// including CRC and DEL
#define ACARSMSG_BUFSIZE	2048

typedef enum {
	ACARS_APP_NONE		= 0,
	ACARS_APP_FANS1A_ADSC	= 1,
	ACARS_APP_FANS1A_CPDLC	= 2
} acars_app_t;

typedef struct {
	uint8_t crc_ok;
	uint8_t mode;
	uint8_t reg[8];
	uint8_t ack;
	uint8_t label[3];
	uint8_t bid;
	uint8_t bs;
	uint8_t no[5];
	uint8_t fid[7];
	char txt[ACARSMSG_BUFSIZE];
	acars_app_t application;
	void *data;
} acars_msg_t;

acars_msg_t *parse_acars(uint8_t *buf, uint32_t len, uint32_t *msg_type);
void output_acars(const acars_msg_t *msg);
