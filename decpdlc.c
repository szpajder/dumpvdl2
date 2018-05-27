/*
 *  decpdlc - a simple FANS-1/A CPDLC message decoder
 *
 *  Copyright (c) 2018 Tomasz Lemiech <szpajder@gmail.com>
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "dumpvdl2.h"
#include "cpdlc.h"

FILE *outf;
uint32_t msg_filter = MSGFLT_ALL;
size_t slurp_hexstring(char* string, uint8_t **buf);
uint8_t dump_asn1 = 0;

void usage() {
	fprintf(stderr,
	"decpdlc version %s\n"
	"(c) 2018 Tomasz Lemiech <szpajder@gmail.com>\n"
	"A little utility for decoding FANS-1/A CPDLC messages embedded in ACARS text\n\n"
	"Usage:\n\n"
	"To decode a single message from command line:\n\n"
	"\t./decpdlc <direction> <acars_message_text>\n\n"
	"where <direction> is one of:\n"
	"\tu - means \"uplink\" (ground-to-air message)\n"
	"\td - means \"downlink\" (air-to-ground message)\n\n"
	"Enclose ACARS message text in quotes if it contains spaces or other shell\n"
	"special shell characters, like '#'.\n\n"
	"Example: ./decpdlc u '- #MD/AA ATLTWXA.CR1.N7881A203A44E8E5C1A932E80E'\n\n"
	"To decode multiple messages from a text file:\n\n"
	"1. Prepare a file with multiple messages, one per line. Precede each line\n"
	"   with 'u' or 'd' (to indicate message direction) and a space. Direction\n"
	"   indicator must appear as a first character on the line (no preceding\n"
	"   spaces please). Example:\n\n"
	"u /AKLCDYA.AT1.9M-MTB215B659D84995674293583561CB9906744E9AF40F9EB\n"
	"u /AKLCDYA.AT1.B-27372142ABDD84A7066418F583561CB9906744E9AF405DA1\n"
	"d /MSTEC7X.AT1.VT-ANE21409DCC3DD03BB52350490502B2E5129D5A15692BA009A08892E7CC831E210A4C06EEBC28B1662BC02360165C80E1F7\n"
	"u - #MD/AA ATLTWXA.CR1.N856DN203A3AA8E5C1A9323EDD\n\n"
	"2. Run decpdlc and pipe the the file contents on standard input:\n\n"
	"\t./decpdlc < cpdlc_messages.txt\n\n"
	"Supported FANS-1/A message types: CR1, CC1, DR1, AT1\n",
	DUMPVDL2_VERSION);
}

void parse(char *txt, uint32_t *msg_type) {
	void *ptr = NULL;
	cpdlc_msgid_t msgid = CPDLC_MSG_UNKNOWN;
	char *s = strstr(txt, ".AT1");
	if(s != NULL) {
		msgid = CPDLC_MSG_AT1;
		goto msgid_set;
	}
	s = strstr(txt, ".CR1");
	if(s != NULL) {
		msgid = CPDLC_MSG_CR1;
		goto msgid_set;
	}
	s = strstr(txt, ".CC1");
	if(s != NULL) {
		msgid = CPDLC_MSG_CC1;
		goto msgid_set;
	}
	s = strstr(txt, ".DR1");
	if(s != NULL) {
		msgid = CPDLC_MSG_DR1;
		goto msgid_set;
	}
msgid_set:
	if(msgid == CPDLC_MSG_UNKNOWN) {
		fprintf(stderr, "not a FANS-1/A CPDLC message\n");
		return;
	}
	s += 4;
	if(strlen(s) < 7) {
		fprintf(stderr, "regnr not found\n");
		goto end;
	}
	s += 7;
	uint8_t *buf = NULL;
	size_t buflen = slurp_hexstring(s, &buf);
	ptr = cpdlc_parse_msg(msgid, buf, (size_t)buflen, msg_type);
end:
	outf = stdout;
	fprintf(outf, "%s\n", txt);
	if(ptr != NULL) {
		cpdlc_output_msg(ptr);
		fprintf(outf, "\n");
	}
	free(buf);
}

int main(int argc, char **argv) {
	uint32_t msg_type = 0;
	if(argc > 1 && !strcmp(argv[1], "-h")) {
		usage();
		exit(0);
	} else if(argc < 2) {
		fprintf(stderr,
			"No command line options found - reading messages from standard input.\n"
			"Use '-h' option for help.\n"
		);

		char buf[1024];
		for(;;) {
			memset(buf, 0, sizeof(buf));
			msg_type = 0;
			if(fgets(buf, sizeof(buf), stdin) == NULL)
				break;
			char *end = strchr(buf, '\n');
			if(end)
				*end = '\0';
			if(strlen(buf) < 3 || (buf[0] != 'u' && buf[0] != 'd') || buf[1] != ' ') {
				fprintf(stderr, "Garbled input: expecting 'u|d acars_message_text'\n");
				continue;
			}
			if(buf[0] == 'u')
				msg_type |= MSGFLT_SRC_GND;
			else if(buf[0] == 'd')
				msg_type |= MSGFLT_SRC_AIR;
			parse(buf, &msg_type);
		}
	} else if(argc == 3) {
		if(argv[1][0] == 'u')
			msg_type |= MSGFLT_SRC_GND;
		else if(argv[1][0] == 'd')
			msg_type |= MSGFLT_SRC_AIR;
		else {
			fprintf(stderr, "Invalid command line options\n\n");
			usage();
			exit(1);
		}
		parse(argv[2], &msg_type);
	} else {
		fprintf(stderr, "Invalid command line options\n\n");
		usage();
		exit(1);
	}
}
