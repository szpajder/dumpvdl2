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
#define _POSIX_C_SOURCE 201112L	/* getaddrinfo */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "dumpvdl2.h"

FILE *outf;
int pp_sockfd = 0;
uint8_t hourly = 0, daily = 0, utc = 0;
static char *filename_prefix = NULL;
static size_t prefix_len;
static struct tm current_tm;

static int open_outfile() {
	char *filename, *fmt;
	size_t tlen;

	if(hourly || daily) {
		time_t t = time(NULL);
		if(utc)
			gmtime_r(&t, &current_tm);
		else
			localtime_r(&t, &current_tm);
		char suffix[16];
		if(hourly)
			fmt = "_%Y%m%d_%H";
		else	// daily
			fmt = "_%Y%m%d";
		tlen = strftime(suffix, sizeof(suffix), fmt, &current_tm);
		if(tlen == 0) {
			fprintf(stderr, "open_outfile(): strfime returned 0\n");
			return -1;
		}
		filename = XCALLOC(prefix_len + tlen + 2, sizeof(uint8_t));
		sprintf(filename, "%s%s", filename_prefix, suffix);
	} else {
		filename = filename_prefix;
	}

	if((outf = fopen(filename, "a+")) == NULL) {
		fprintf(stderr, "Could not open output file %s: %s\n", filename, strerror(errno));
		return -1;
	}
	if(hourly || daily)
		free(filename);

	return 0;
}

int init_output_file(char *file) {
	if(!strcmp(file, "-")) {
		outf = stdout;
	} else {
		filename_prefix = file;
		prefix_len = strlen(filename_prefix);
		return open_outfile();
	}
	return 0;
}

int init_pp(char *pp_addr) {
	if(pp_addr == NULL) return -1;

	char *addr, *port;
	if((addr = strtok(pp_addr, ":")) == NULL)
		return -1;
	if((port = strtok(NULL, ":")) == NULL)
		return -1;

	struct addrinfo hints, *result, *rptr;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	int ret = getaddrinfo(addr, port, &hints, &result);
	if(ret != 0) {
		fprintf(stderr, "Could not resolve %s: %s\n", pp_addr, gai_strerror(ret));
		return -1;
	}
	for (rptr = result; rptr != NULL; rptr = rptr->ai_next) {
		pp_sockfd = socket(rptr->ai_family, rptr->ai_socktype, rptr->ai_protocol);
		if (pp_sockfd == -1) continue;
		if (connect(pp_sockfd, rptr->ai_addr, rptr->ai_addrlen) != -1) break;
		close(pp_sockfd);
	}
	if (rptr == NULL) {
		fprintf(stderr, "Could not connect to Planeplotter: all addresses failed\n");
		return -1;
	}
	freeaddrinfo(result);
	return 0;
}

int rotate_outfile() {
	struct tm new_tm;
	time_t t = time(NULL);
	if(utc)
		gmtime_r(&t, &new_tm);
	else
		localtime_r(&t, &new_tm);
	if((hourly && new_tm.tm_hour != current_tm.tm_hour) || (daily && new_tm.tm_mday != current_tm.tm_mday)) {
		fclose(outf);
		return open_outfile();
	}
	return 0;
}

void output_raw(uint8_t *buf, uint32_t len) {
	if(len == 0)
		return;
	fprintf(outf, "   ");
	for(int i = 0; i < len; i++)
		fprintf(outf, "%02x ", buf[i]);
	fprintf(outf, "\n");
}
