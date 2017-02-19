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
#if WITH_MIRISDR
#include "mirisdr.h"
#endif
#include "dumpvdl2.h"

int do_exit = 0;

void sighandler(int sig) {
	fprintf(stderr, "Got signal %d, exiting\n", sig);
	do_exit = 1;
#if WITH_RTLSDR
	rtl_cancel();
#endif
#if WITH_MIRISDR
	mirisdr_cancel();
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

static uint32_t calc_centerfreq(uint32_t *freq, int cnt, uint32_t source_rate) {
// FIXME
	return freq[0];
}

void process_file(vdl2_state_t *ctx, char *path, enum sample_formats sfmt) {
	FILE *f;
	uint32_t len;
	unsigned char buf[FILE_BUFSIZE];
	void (*process_buf)() = NULL;
	
	if((f = fopen(path, "r")) == NULL) {
		perror("fopen()");
		_exit(2);
	}
	switch(sfmt) {
	case SFMT_U8:
		process_buf_uchar_init();
		ctx->sbuf = XCALLOC(FILE_BUFSIZE / sizeof(uint8_t), sizeof(float));
		process_buf = &process_buf_uchar;
		break;
	case SFMT_S16_LE:
		ctx->sbuf = XCALLOC(FILE_BUFSIZE / sizeof(int16_t), sizeof(float));
		process_buf = &process_buf_short;
		break;
	default:
		fprintf(stderr, "Unsupported sample format\n");
		_exit(5);
	}
	do {
		len = fread(buf, 1, FILE_BUFSIZE, f);
		(*process_buf)(buf, len, ctx);
	} while(len == FILE_BUFSIZE && !do_exit);
	fclose(f);
}

void usage() {
	fprintf(stderr, "DUMPVDL2 version %s\n", DUMPVDL2_VERSION);
	fprintf(stderr, "Usage:\n\n");
#if WITH_RTLSDR
	fprintf(stderr, "RTL-SDR receiver:\n");
	fprintf(stderr, "\tdumpvdl2 [output_options] -R <device_id> [rtlsdr_options] [<freq_1> [freq_2 [...]]]\n");
#endif
#if WITH_MIRISDR
	fprintf(stderr, "MIRI-SDR receiver:\n");
	fprintf(stderr, "\tdumpvdl2 [output_options] -M <device_id> [mirisdr_options] [<freq_1> [freq_2 [...]]]\n");
#endif
	fprintf(stderr, "I/Q input from file:\n");
	fprintf(stderr, "\tdumpvdl2 [output_options] -F <input_file> [file_options] [<freq_1> [freq_2 [...]]]\n");
	fprintf(stderr, "\ncommon options:\n");
	fprintf(stderr, "\t<freq_n>\tVDL2 channel frequency, in Hz (max %d simultaneous channels supported).\n", MAX_CHANNELS);
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
	fprintf(stderr, "\t-g <gain>\t\tSet gain (decibels)\n");
	fprintf(stderr, "\t-p <correction>\t\tSet freq correction (ppm)\n");
	fprintf(stderr, "\t-c <center_frequency>\tSet center frequency in Hz (default: auto)\n");
#endif
#if WITH_MIRISDR
	fprintf(stderr, "\nmirisdr_options:\n");
	fprintf(stderr, "\t-M <device_id>\t\tUse Mirics device with specified ID (default: 0)\n");
	fprintf(stderr, "\t-T <device_type>\t0 - default, 1 - SDRPlay\n");
	fprintf(stderr, "\t-g <gain>\t\tSet gain (in decibels, from 0 to 102 dB)\n");
	fprintf(stderr, "\t-p <correction>\t\tSet freq correction (in Hertz)\n");
	fprintf(stderr, "\t-c <center_frequency>\tSet center frequency in Hz (default: auto)\n");
	fprintf(stderr, "\t-e <usb_transfer_mode>\t0 - isochronous (default), 1 - bulk\n");
#endif
	fprintf(stderr, "\nfile_options:\n");
	fprintf(stderr, "\t-F <input_file>\t\tRead I/Q samples from file\n");
	fprintf(stderr, "\t-c <center_frequency>\tCenter frequency of the input data, in Hz (default: 0)\n");
	fprintf(stderr, "\t-O <oversample_rate>\tOversampling rate for recorded data (default: %u)\n", FILE_OVERSAMPLE);
	fprintf(stderr, "\t\t\t\t  (sampling rate will be set to %u * oversample_rate)\n", SYMBOL_RATE * SPS);
	fprintf(stderr, "\t-m <sample_format>\tInput sample format. Supported formats:\n");
	fprintf(stderr, "\t\t\t\t  U8\t\t8-bit unsigned (eg. recorded with rtl_sdr) (default)\n");
	fprintf(stderr, "\t\t\t\t  S16_LE\t16-bit signed, little-endian (eg. recorded with miri_sdr)\n");
	_exit(0);
}

int main(int argc, char **argv) {
	vdl2_state_t ctx;
	uint32_t centerfreq = 0, sample_rate = 0, oversample = 0;
	uint32_t *freqs;
	int num_channels = 0;
	enum input_types input = INPUT_UNDEF;
	enum sample_formats sample_fmt = SFMT_UNDEF;
#if WITH_RTLSDR || WITH_MIRISDR
	uint32_t device = 0;
	float gain = SDR_AUTO_GAIN;
	int correction = 0;
#endif
#if WITH_MIRISDR
	int mirisdr_hw_flavour = 0;
	int mirisdr_usb_xfer_mode = 0;
#endif
	int opt;
	char *optstring = "c:De:F:g:hHm:M:o:O:p:R:S:T:";
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
			sample_fmt = SFMT_U8;
			break;
		case 'm':
			if(!strcmp(optarg, "U8"))
				sample_fmt = SFMT_U8;
			else if(!strcmp(optarg, "S16_LE"))
				sample_fmt = SFMT_S16_LE;
			else {
				fprintf(stderr, "Unknown sample format\n");
				_exit(1);
			}
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
#if WITH_MIRISDR
		case 'M':
			device = strtoul(optarg, NULL, 10);
			input = INPUT_MIRISDR;
			oversample = MIRISDR_OVERSAMPLE;
			break;
		case 'T':
			mirisdr_hw_flavour = atoi(optarg);
			break;
		case 'e':
			mirisdr_usb_xfer_mode = atoi(optarg);
			break;
#endif
#if WITH_RTLSDR
		case 'R':
			device = strtoul(optarg, NULL, 10);
			input = INPUT_RTLSDR;
			oversample = RTL_OVERSAMPLE;
			break;
#endif
#if WITH_RTLSDR || WITH_MIRISDR
		case 'g':
			gain = atof(optarg);
			break;
		case 'p':
			correction = atoi(optarg);
			break;
#endif
		case 'o':
			outfile = strdup(optarg);
			break;
		case 'O':
			oversample = atoi(optarg);
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
	if(input == INPUT_UNDEF)
		usage();

	if(optind < argc) {
		num_channels = argc - optind;
		if(num_channels > MAX_CHANNELS) {
			fprintf(stderr, "Error: too many channels specified (%d > %d)\n", num_channels, MAX_CHANNELS);
			_exit(1);
		}
		freqs = XCALLOC(num_channels, sizeof(uint32_t));
		for(int i = 0; i < num_channels; i++)
			freqs[i] = strtoul(argv[optind+i], NULL, 10);
	} else {
		fprintf(stderr, "Warning: frequency not set - using VDL2 Common Signalling Channel as a default (%u Hz)\n", CSC_FREQ);
		num_channels = 1;
		freqs = XCALLOC(num_channels, sizeof(uint32_t));
		freqs[0] = CSC_FREQ;
	}

	if(outfile == NULL) {
		outfile = strdup("-");		// output to stdout by default
		hourly = daily = 0;		// stdout is not rotateable - ignore silently
	}
	if(outfile != NULL && hourly && daily) {
		fprintf(stderr, "Options: -H and -D are exclusive\n");
		fprintf(stderr, "Use -h for help\n");
		_exit(1);
	}
	sample_rate = SYMBOL_RATE * SPS * oversample;
	fprintf(stderr, "Sampling rate set to %u sps\n", sample_rate);
	if(centerfreq == 0) {
		centerfreq = calc_centerfreq(freqs, num_channels, sample_rate);
		if(centerfreq == 0) {
			fprintf(stderr, "Failed to calculate center frequency\n");
			_exit(2);
		}
	}

	memset(&ctx, 0, sizeof(vdl2_state_t));
	ctx.num_channels = num_channels;
	ctx.channels = XCALLOC(num_channels, sizeof(vdl2_channel_t *));
	for(int i = 0; i < num_channels; i++) {
		if((ctx.channels[i] = vdl2_channel_init(centerfreq, freqs[i], sample_rate, oversample)) == NULL) {
			fprintf(stderr, "Failed to initialize VDL channel\n");
			_exit(2);
		}
	}

	if(rs_init() < 0) {
		fprintf(stderr, "Failed to initialize RS codec\n");
		_exit(3);
	}
#if USE_STATSD
	if(statsd_enabled && input != INPUT_FILE) {
		if(statsd_initialize(statsd_addr) < 0) {
				fprintf(stderr, "Failed to initialize statsd client\n");
				_exit(4);
		}
		for(int i = 0; i < num_channels; i++)
			statsd_initialize_counters(freqs[i]);
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
		process_file(&ctx, infile, sample_fmt);
		break;
#if WITH_RTLSDR
	case INPUT_RTLSDR:
		rtl_init(&ctx, device, centerfreq, gain, correction);
		break;
#endif
#if WITH_MIRISDR
	case INPUT_MIRISDR:
		mirisdr_init(&ctx, device, mirisdr_hw_flavour, centerfreq, gain, correction, mirisdr_usb_xfer_mode);
		break;
#endif
	default:
		fprintf(stderr, "Unknown input type\n");
		_exit(5);
	}
	return(0);
}
