#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#if WITH_RTLSDR
#include "rtl.h"
#endif
#include "dumpvdl2.h"

int do_exit = 0;

void sighandler(int sig) {
	fprintf(stderr, "Got signal %d, exiting\n", sig);
	do_exit = 1;
#if WITH_RTLSDR
	rtl_cancel();
#endif
}

void setup_signals() {
	struct sigaction sigact, pipeact;

	memset(&sigact, 0, sizeof(sigact));
	memset(&pipeact, 0, sizeof(pipeact));
	pipeact.sa_handler = SIG_IGN;
	sigact.sa_handler = &sighandler;
	sigaction(SIGPIPE, &pipeact, NULL);
	sigaction(SIGHUP, &sigact, NULL);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
}

static uint32_t calc_centerfreq(uint32_t freq, uint32_t source_rate) {
	return freq;
}

void process_file(vdl2_state_t *ctx, char *path) {
	FILE *f;
	uint32_t len;
	unsigned char buf[FILE_BUFSIZE];
	
	if((f = fopen(path, "r")) == NULL) {
		perror("fopen()");
		_exit(2);
	}
	process_buf_uchar_init();
	ctx->sbuf = XCALLOC(RTL_BUFSIZE, sizeof(float));
	do {
		len = fread(buf, 1, FILE_BUFSIZE, f);
		process_buf_uchar(buf, len, ctx);
	} while(len == FILE_BUFSIZE && !do_exit);
	fclose(f);
}

void usage() {
	fprintf(stderr, "DUMPVDL2 version %s\n", DUMPVDL2_VERSION);
	fprintf(stderr, "Usage:\n\n");
#if WITH_RTLSDR
	fprintf(stderr, "RTLSDR receiver:\n");
	fprintf(stderr, "\tdumpvdl2 [output_options] -R <device_id> [rtlsdr_options] [<channel_frequency>]\n");
#endif
	fprintf(stderr, "I/Q input from file:\n");
	fprintf(stderr, "\tdumpvdl2 [output_options] -F <input_file> [file_options] [<channel_frequency>]\n");
	fprintf(stderr, "\ncommon options:\n");
	fprintf(stderr, "\t<channel_frequency>\tVDL2 channel to listen to, In Hz.\n");
	fprintf(stderr, "\t\t\t\tIf omitted, will use VDL2 Common Signalling Channel (%u Hz)\n", CSC_FREQ);
	fprintf(stderr, "\noutput_options:\n");
	fprintf(stderr, "\t-o <output_file>\tOutput decoded frames to <output_file> (default: stdout)\n");
	fprintf(stderr, "\t-H\t\t\tRotate output file hourly\n");
	fprintf(stderr, "\t-D\t\t\tRotate output file daily\n");
#if USE_STATSD
	fprintf(stderr, "\t-S <host>:<port>\tSend statistics to Etsy StatsD server <host>:<port> (default: disabled)\n");
#endif
#if WITH_RTLSDR
	fprintf(stderr, "\nrtlsdr_options:\n");
	fprintf(stderr, "\t-R <device_id>\t\tUse RTL device with specified ID (default: 0)\n");
	fprintf(stderr, "\t-g <gain>\t\tSet RTL gain (decibels)\n");
	fprintf(stderr, "\t-p <correction>\t\tSet RTL freq correction (ppm)\n");
	fprintf(stderr, "\t-c <center_frequency>\tSet RTL center frequency in Hz (default: auto)\n");
#endif
	fprintf(stderr, "\nfile_options:\n");
	fprintf(stderr, "\t-F <input_file>\t\tRead I/Q samples from file (must be sampled at %u samples/sec)\n", SYMBOL_RATE * SPS * FILE_OVERSAMPLE);
	fprintf(stderr, "\t-c <center_frequency>\tCenter frequency of the input data, in Hz (default: 0)\n");
	_exit(0);
}

int main(int argc, char **argv) {
	vdl2_state_t *ctx;
	uint32_t freq = 0, centerfreq = 0, sample_rate = 0, oversample = 0;
	enum input_types input = INPUT_UNDEF;
#if WITH_RTLSDR
	uint32_t device = 0;
// FIXME: default gain and correction depend on receiver type which is currently enabled
	int gain = RTL_AUTO_GAIN;
	int correction = 0;
#endif
	int opt;
	char *optstring = "c:DF:g:hHo:p:R:S:";
#if USE_STATSD
	char *statsd_addr = NULL;
	int statsd_enabled = 0;
#endif
	char *infile = NULL, *outfile = NULL;

	while((opt = getopt(argc, argv, optstring)) != -1) {
		switch(opt) {
		case 'F':
			infile = strdup(optarg);
			input = INPUT_FILE;
			oversample = FILE_OVERSAMPLE;
			break;
		case 'H':
			hourly = 1;
			break;
		case 'D':
			daily = 1;
			break;
		case 'c':
			centerfreq = strtoul(optarg, NULL, 10);
			break;
#if WITH_RTLSDR
		case 'R':
			device = strtoul(optarg, NULL, 10);
			input = INPUT_RTLSDR;
			oversample = RTL_OVERSAMPLE;
			break;
		case 'g':
			gain = (int)(10 * atof(optarg));
			break;
		case 'p':
			correction = atoi(optarg);
			break;
#endif
		case 'o':
			outfile = strdup(optarg);
			break;
#if USE_STATSD
		case 'S':
			statsd_addr = strdup(optarg);
			statsd_enabled = 1;
			break;
#endif
		case 'h':
		default:
			usage();
		}
	}
	if(optind < argc)
		freq = strtoul(argv[optind], NULL, 10);

	if(input == INPUT_UNDEF)
		usage();
	if(outfile == NULL) {
		outfile = strdup("-");		// output to stdout by default
		hourly = daily = 0;		// stdout is not rotateable - ignore silently
	}
	if(outfile != NULL && hourly && daily) {
		fprintf(stderr, "Options: -H and -D are exclusive\n");
		fprintf(stderr, "Use -h for help\n");
		_exit(1);
	}
	if(freq == 0) {
		fprintf(stderr, "Warning: frequency not set - using VDL2 Common Signalling Channel as a default (%u Hz)\n", CSC_FREQ);
		freq = CSC_FREQ;
	}
	sample_rate = SYMBOL_RATE * SPS * oversample;
	fprintf(stderr, "Sampling rate set to %u symbols/sec\n", sample_rate);
	if(centerfreq == 0) {
		centerfreq = calc_centerfreq(freq, sample_rate);
		if(centerfreq == 0) {
			fprintf(stderr, "Failed to calculate center frequency\n");
			_exit(2);
		}
	}
	if((ctx = vdl2_init(centerfreq, freq, sample_rate, oversample)) == NULL) {
		fprintf(stderr, "Failed to initialize VDL state\n");
		_exit(2);
	}
	if(rs_init() < 0) {
		fprintf(stderr, "Failed to initialize RS codec\n");
		_exit(3);
	}
#if USE_STATSD
	if(statsd_enabled && freq != 0) {
		if(statsd_initialize(statsd_addr) < 0) {
				fprintf(stderr, "Failed to initialize statsd client\n");
				_exit(4);
		}
		statsd_initialize_counters(freq);
	} else {
		statsd_enabled = 0;
	}
#endif
	if(init_output_file(outfile) < 0) {
		fprintf(stderr, "Failed to initialize output - aborting\n");
		_exit(4);
	}
	setup_signals();
	sincosf_lut_init();
	switch(input) {
	case INPUT_FILE:
		process_file(ctx, infile);
		break;
#if WITH_RTLSDR
	case INPUT_RTLSDR:
		rtl_init(ctx, device, centerfreq, gain, correction);
		break;
#endif
	default:
		fprintf(stderr, "Unknown input type\n");
		_exit(5);
	}
	return(0);
}
