#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "dumpvdl2.h"

FILE *outf;
uint8_t hourly = 0, daily = 0;
static char *filename_prefix = NULL;
static size_t prefix_len;
static struct tm current_tm;

static int open_outfile() {
	char *filename, *fmt;
	size_t tlen;

	if(hourly || daily) {
		time_t t = time(NULL);
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

int rotate_outfile() {
	struct tm new_tm;
	time_t t = time(NULL);
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
