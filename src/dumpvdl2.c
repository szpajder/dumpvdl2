/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
 *  Copyright (c) 2017-2020 Tomasz Lemiech <szpajder@gmail.com>
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>
#include <libacars/libacars.h>  // LA_VERSION, la_config_set_bool()
#include <libacars/acars.h>     // LA_ACARS_BEARER_VHF
#include <libacars/list.h>      // la_list
#include <pthread.h>
#include "config.h"
#include "kvargs.h"
#include "output-common.h"
#include "decode.h"             // avlc_decoder_thread, avlc_decoder_shutdown, avlc_decoder_init
#ifndef HAVE_PTHREAD_BARRIERS
#include "pthread_barrier.h"
#endif
#ifdef WITH_PROFILING
#include <gperftools/profiler.h>
#endif
#ifdef WITH_RTLSDR
#include "rtl.h"
#endif
#ifdef WITH_MIRISDR
#include "mirics.h"
#endif
#ifdef WITH_SDRPLAY
#include "sdrplay.h"
#endif
#ifdef WITH_SDRPLAY3
#include "sdrplay3.h"
#endif
#ifdef WITH_SOAPYSDR
#include "soapysdr.h"
#endif
#include "dumpvdl2.h"
#ifdef WITH_SQLITE
#include "ac_data.h"
#endif
#include "gs_data.h"

int do_exit = 0;
dumpvdl2_config_t Config;

pthread_barrier_t demods_ready, samples_ready;

void sighandler(int sig) {
	fprintf(stderr, "Got signal %d, ", sig);
	if(do_exit == 0) {
		fprintf(stderr, "exiting gracefully (send signal once again to force quit)\n");
	} else {
		fprintf(stderr, "forcing quit\n");
	}
	do_exit++;
#ifdef WITH_RTLSDR
	rtl_cancel();
#endif
#ifdef WITH_MIRISDR
	mirisdr_cancel();
#endif
#ifdef WITH_SDRPLAY
	sdrplay_cancel();
#endif
#ifdef WITH_SDRPLAY3
	sdrplay3_cancel();
#endif
#ifdef WITH_SOAPYSDR
	soapysdr_cancel();
#endif
}

static void setup_signals() {
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

static void pthread_barrier_new(pthread_barrier_t *barrier, unsigned count) {
	int ret;
	if((ret = pthread_barrier_init(barrier, NULL, count)) != 0) {
		errno = ret;
		perror("pthread_barrier_init failed");
		_exit(2);
	}
}

static void setup_barriers(vdl2_state_t *ctx) {
	pthread_barrier_new(&demods_ready, ctx->num_channels+1);
	pthread_barrier_new(&samples_ready, ctx->num_channels+1);
}

void start_thread(pthread_t *pth, void *(*start_routine)(void *), void *thread_ctx) {
	int ret;
	if((ret = pthread_create(pth, NULL, start_routine, thread_ctx) != 0)) {
		errno = ret;
		perror("pthread_create failed");
		_exit(2);
	}
}

static void start_demod_threads(vdl2_state_t *ctx) {
	for(int i = 0; i < ctx->num_channels; i++) {
		start_thread(&ctx->channels[i]->demod_thread, &process_samples, ctx->channels[i]);
	}
}

void start_output_thread(void *p, void *ctx) {
	UNUSED(ctx);
	ASSERT(p != NULL);
	output_instance_t *output = p;
	debug_print(D_OUTPUT, "starting thread for output %s\n", output->td->name);
	start_thread(output->output_thread, output_thread, output);
}

void start_all_output_threads_for_fmtr(void *p, void *ctx) {
	UNUSED(ctx);
	ASSERT(p != NULL);
	fmtr_instance_t *fmtr = p;
	la_list_foreach(fmtr->outputs, start_output_thread, NULL);
}

void start_all_output_threads(la_list *fmtr_list) {
	la_list_foreach(fmtr_list, start_all_output_threads_for_fmtr, NULL);
}

static uint32_t calc_centerfreq(uint32_t *freq, int cnt, uint32_t source_rate) {
	uint32_t freq_min, freq_max;
	freq_min = freq_max = freq[0];
	for(int i = 0; i < cnt; i++) {
		if(freq[i] < freq_min) freq_min = freq[i];
		if(freq[i] > freq_max) freq_max = freq[i];
	}
	if(freq_max - freq_min > source_rate * 0.8f) {
		fprintf(stderr, "Error: given frequencies are too far apart\n");
		return 0;
	}
	return freq_min + (freq_max - freq_min) / 2;
}

typedef struct {
	char *output_spec_string;
	char *intype, *outformat, *outtype;
	kvargs *outopts;
	char const *errstr;
	bool err;
} output_params;

#define SCAN_FIELD_OR_FAIL(str, field_name, errstr) \
	(field_name) = strsep(&(str), ":"); \
	if((field_name)[0] == '\0') { \
		(errstr) = "field_name is empty"; \
		goto fail; \
	} else if((str) == NULL) { \
		(errstr) = "not enough fields"; \
		goto fail; \
	}

output_params output_params_from_string(char *output_spec) {
	output_params out_params = {
		.intype = NULL, .outformat = NULL, .outtype = NULL, .outopts = NULL,
		.errstr = NULL, .err = false
	};

	// We have to work on a copy of output_spec, because strsep() modifies its
	// first argument. The copy gets stored in the returned out_params structure
	// so that the caller can free it later.
	debug_print(D_MISC, "output_spec: %s\n", output_spec);
	out_params.output_spec_string = strdup(output_spec);
	char *ptr = out_params.output_spec_string;

	// output_spec format is: <input_type>:<output_format>:<output_type>:<output_options>
	SCAN_FIELD_OR_FAIL(ptr, out_params.intype, out_params.errstr);
	SCAN_FIELD_OR_FAIL(ptr, out_params.outformat, out_params.errstr);
	SCAN_FIELD_OR_FAIL(ptr, out_params.outtype, out_params.errstr);
	debug_print(D_MISC, "intype: %s outformat: %s outtype: %s\n",
			out_params.intype, out_params.outformat, out_params.outtype);

	debug_print(D_MISC, "kvargs input string: %s\n", ptr);
	kvargs_parse_result outopts = kvargs_from_string(ptr);
	if(outopts.err == 0) {
		out_params.outopts = outopts.result;
	} else {
		out_params.errstr = kvargs_get_errstr(outopts.err);
		goto fail;
	}

	out_params.outopts = outopts.result;
	debug_print(D_MISC, "intype: %s outformat: %s outtype: %s\n",
			out_params.intype, out_params.outformat, out_params.outtype);
	goto end;
fail:
	XFREE(out_params.output_spec_string);
	out_params.err = true;
end:
	return out_params;
}

fmtr_instance_t *find_fmtr_instance(la_list *fmtr_list, fmtr_descriptor_t *fmttd, fmtr_input_type_t intype) {
	if(fmtr_list == NULL) {
		return NULL;
	}
	for(la_list *p = fmtr_list; p != NULL; p = la_list_next(p)) {
		fmtr_instance_t *fmtr = p->data;
		if(fmtr->td == fmttd && fmtr->intype == intype) {
			return fmtr;
		}
	}
	return NULL;
}

la_list *setup_output(la_list *fmtr_list, char *output_spec) {
	if(!strcmp(output_spec, "help")) {
		output_usage();
		_exit(0);
	}
	output_params oparams = output_params_from_string(output_spec);
	if(oparams.err == true) {
		fprintf(stderr, "Could not parse output specifier '%s': %s\n", output_spec, oparams.errstr);
		_exit(1);
	}
	debug_print(D_MISC, "intype: %s outformat: %s outtype: %s\n",
			oparams.intype, oparams.outformat, oparams.outtype);

	fmtr_input_type_t intype = fmtr_input_type_from_string(oparams.intype);
	if(intype == FMTR_INTYPE_UNKNOWN) {
		fprintf(stderr, "Data type '%s' is unknown\n", oparams.intype);
		_exit(1);
	}

	output_format_t outfmt = output_format_from_string(oparams.outformat);
	if(outfmt == OFMT_UNKNOWN) {
		fprintf(stderr, "Output format '%s' is unknown\n", oparams.outformat);
		_exit(1);
	}

	fmtr_descriptor_t *fmttd = fmtr_descriptor_get(outfmt);
	ASSERT(fmttd != NULL);
	fmtr_instance_t *fmtr = find_fmtr_instance(fmtr_list, fmttd, intype);
	if(fmtr == NULL) {      // we haven't added this formatter to the list yet
		if(!fmttd->supports_data_type(intype)) {
			fprintf(stderr,
					"Unsupported data_type:format combination: '%s:%s'\n",
					oparams.intype, oparams.outformat);
			_exit(1);
		}
		fmtr = fmtr_instance_new(fmttd, intype);
		ASSERT(fmtr != NULL);
		fmtr_list = la_list_append(fmtr_list, fmtr);
	}

	output_descriptor_t *otd = output_descriptor_get(oparams.outtype);
	if(otd == NULL) {
		fprintf(stderr, "Output type '%s' is unknown\n", oparams.outtype);
		_exit(1);
	}
	if(!otd->supports_format(outfmt)) {
		fprintf(stderr, "Unsupported format:output combination: '%s:%s'\n",
				oparams.outformat, oparams.outtype);
		_exit(1);
	}

	void *output_cfg = otd->configure(oparams.outopts);
	if(output_cfg == NULL) {
		fprintf(stderr, "Invalid output configuration\n");
		_exit(1);
	}

	output_instance_t *output = output_instance_new(otd, outfmt, output_cfg);
	ASSERT(output != NULL);
	fmtr->outputs = la_list_append(fmtr->outputs, output);

	// oparams is no longer needed after this point.
	// No need to free intype, outformat and outtype fields, because they
	// point into output_spec_string.
	XFREE(oparams.output_spec_string);
	kvargs_destroy(oparams.outopts);

	return fmtr_list;
}

void process_iq_file(vdl2_state_t *ctx, char *path, enum sample_formats sfmt) {
	UNUSED(ctx);
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
			sbuf = XCALLOC(FILE_BUFSIZE / sizeof(uint8_t), sizeof(float));
			process_buf = &process_buf_uchar;
			break;
		case SFMT_S16_LE:
			sbuf = XCALLOC(FILE_BUFSIZE / sizeof(int16_t), sizeof(float));
			process_buf = &process_buf_short;
			break;
		default:
			fprintf(stderr, "Unsupported sample format\n");
			_exit(5);
	}
	do {
		len = fread(buf, 1, FILE_BUFSIZE, f);
		(*process_buf)(buf, len, NULL);
	} while(len == FILE_BUFSIZE && do_exit == 0);
	fclose(f);
}

void print_version() {
	fprintf(stderr, "dumpvdl2 %s (libacars %s)\n", DUMPVDL2_VERSION, LA_VERSION);
}

void describe_option(char const *name, char const *description, int indent) {
	int descr_shiftwidth = USAGE_OPT_NAME_COLWIDTH - (int)strlen(name) - indent * USAGE_INDENT_STEP;
	if(descr_shiftwidth < 1) {
		descr_shiftwidth = 1;
	}
	fprintf(stderr, "%*s%s%*s%s\n", IND(indent), "", name, descr_shiftwidth, "", description);
}


void usage() {
	fprintf(stderr, "Usage:\n");
#ifdef WITH_RTLSDR
	fprintf(stderr, "\nRTL-SDR receiver:\n\n"
			"%*sdumpvdl2 [output_options] --rtlsdr <device_id> [rtlsdr_options] [<freq_1> [<freq_2> [...]]]\n",
			IND(1), "");
#endif
#ifdef WITH_MIRISDR
	fprintf(stderr, "\nMIRI-SDR receiver:\n\n"
			"%*sdumpvdl2 [output_options] --mirisdr <device_id> [mirisdr_options] [<freq_1> [<freq_2> [...]]]\n",
			IND(1), "");
#endif
#ifdef WITH_SDRPLAY
	fprintf(stderr, "\nSDRPLAY RSP receiver (using API version 2):\n\n"
			"%*sdumpvdl2 [output_options] --sdrplay <device_id> [sdrplay_options] [<freq_1> [<freq_2> [...]]]\n",
			IND(1), "");
#endif
#ifdef WITH_SDRPLAY3
	fprintf(stderr, "\nSDRPLAY RSP receiver (using API version 3):\n\n"
			"%*sdumpvdl2 [output_options] --sdrplay3 <device_id> [sdrplay3_options] [<freq_1> [<freq_2> [...]]]\n",
			IND(1), "");
#endif
#ifdef WITH_SOAPYSDR
	fprintf(stderr, "\nSOAPYSDR compatible receiver:\n\n"
			"%*sdumpvdl2 [output_options] --soapysdr <device_id> [soapysdr_options] [<freq_1> [<freq_2> [...]]]\n",
			IND(1), "");
#endif
	fprintf(stderr, "\nRead I/Q samples from file:\n\n"
			"%*sdumpvdl2 [output_options] --iq-file <input_file> [file_options] [<freq_1> [<freq_2> [...]]]\n",
			IND(1), "");
#ifdef WITH_PROTOBUF_C
	fprintf(stderr, "\nRead raw AVLC frames from file:\n\n"
			"%*sdumpvdl2 [output_options] --raw-frames-file <input_file>\n",
			IND(1), "");
#endif
	fprintf(stderr, "\nGeneral options:\n");
	describe_option("--help", "Displays this text", 1);
	describe_option("--version", "Displays program version number", 1);
#ifdef DEBUG
	describe_option("--debug <filter_spec>", "Debug message classes to display (default: none) (\"--debug help\" for details)", 1);
#endif
	fprintf(stderr, "common options:\n");
	describe_option("<freq_1> [<freq_2> [...]]", "VDL2 channel frequencies, in Hz", 1);
	fprintf(stderr, "\nMaximum number of simultaneous VDL2 channels supported is %d.\n", MAX_CHANNELS);
	fprintf(stderr, "If channel frequencies are omitted, VDL2 Common Signalling Channel (%u Hz) will be used as default.\n\n", CSC_FREQ);

#ifdef WITH_RTLSDR
	fprintf(stderr, "rtlsdr_options:\n");
	describe_option("--rtlsdr <device_id>", "Use RTL device with specified ID or serial number (default: ID=0)", 1);
	describe_option("--gain <gain>", "Set gain (decibels)", 1);
	describe_option("--correction <correction>", "Set freq correction (ppm)", 1);
	describe_option("--centerfreq <center_frequency>", "Set center frequency in Hz (default: auto)", 1);
#endif
#ifdef WITH_MIRISDR
	fprintf(stderr, "\nmirisdr_options:\n");
	describe_option("--mirisdr <device_id>", "Use Mirics device with specified ID or serial number (default: ID=0)", 1);
	describe_option("--hw-type <device_type>", "0 - default, 1 - SDRPlay", 1);
	describe_option("--gain <gain>", "Set gain (in decibels, from 0 to 102 dB)", 1);
	describe_option("--correction <correction>", "Set freq correction (in Hertz)", 1);
	describe_option("--centerfreq <center_frequency>", "Set center frequency in Hz (default: auto)", 1);
	describe_option("--usb-mode <usb_transfer_mode>", "0 - isochronous (default), 1 - bulk", 1);
#endif
#ifdef WITH_SDRPLAY
	fprintf(stderr, "\nsdrplay_options:\n");
	describe_option("--sdrplay <device_id>", "Use SDRPlay RSP device with specified ID or serial number (default: ID=0)", 1);
	describe_option("--gr <gr>", "Set system gain reduction, in dB, positive (if omitted, auto gain is enabled)", 1);
	describe_option("--agc <AGC_set_point>", "Auto gain set point in dBFS, negative (default: -30)", 1);
	describe_option("--correction <correction>", "Set freq correction (ppm)", 1);
	describe_option("--centerfreq <center_frequency>", "Set center frequency in Hz (default: auto)", 1);
	describe_option("--antenna <A/B>", "RSP2 antenna port selection (default: A)", 1);
	describe_option("--biast <0/1>", "RSP2/1a/duo Bias-T control: 0 - off (default), 1 - on", 1);
	describe_option("--notch-filter <0/1>", "RSP2/1a/duo AM/FM/bcast notch filter control: 0 - off (default), 1 - on", 1);
	describe_option("--tuner <1/2>", "RSPduo tuner selection: (default: 1)", 1);
#endif
#ifdef WITH_SDRPLAY3
	fprintf(stderr, "\nsdrplay3_options:\n");
	describe_option("--sdrplay3 <device_id>", "Use SDRPlay RSP device with specified ID or serial number (default: ID=0)", 1);
	describe_option("--ifgr <IF_gain_reduction>", "Set IF gain reduction, in dB, positive (if omitted, auto gain is enabled)", 1);
	describe_option("--lna-state <LNA_state>", "Set LNA state, non-negative, higher state = higher gain reduction", 1);
	describe_option("", "(if omitted, auto gain is enabled)", 1);
	describe_option("--agc <AGC_set_point>", "Auto gain set point in dBFS, negative (default: -30)", 1);
	describe_option("--correction <correction>", "Set freq correction (ppm)", 1);
	describe_option("--centerfreq <center_frequency>", "Set center frequency in Hz (default: auto)", 1);
	describe_option("--antenna <A/B/C>", "RSP2/dx antenna port selection (default: A)", 1);
	describe_option("--biast <0/1>", "RSP2/1a/duo/dx Bias-T control: 0 - off (default), 1 - on", 1);
	describe_option("--notch-filter <0/1>", "RSP2/1a/duo/dx AM/FM/bcast notch filter control: 0 - off (default), 1 - on", 1);
	describe_option("--dab-notch-filter <0/1>", "RSP1a/duo/dx DAB notch filter control: 0 - off (default), 1 - on", 1);
	describe_option("--tuner <1/2>", "RSPduo tuner selection: (default: 1)", 1);
#endif
#ifdef WITH_SOAPYSDR
	fprintf(stderr, "\nsoapysdr_options:\n");
	describe_option("--soapysdr <device_id>", "Use SoapySDR compatible device with specified ID (default: ID=0)", 1);
	describe_option("--device-settings <key1=val1,key2=val2,...>", "Set device-specific parameters (default: none)", 1);
	describe_option("--gain <gain>", "Set gain (decibels)", 1);
	describe_option("--correction <correction>", "Set freq correction (ppm)", 1);
	describe_option("--soapy-antenna <antenna>", "Set antenna port selection (default: RX)", 1);
	describe_option("--soapy-gain <gain1=val1,gain2=val2,...>", "Set gain components (default: none)", 1);
#endif
	fprintf(stderr, "\nfile_options:\n");
	describe_option("--iq-file <input_file>", "Read I/Q samples from file", 1);
	describe_option("--centerfreq <center_frequency>", "Center frequency of the input data, in Hz (default: 0)", 1);
	describe_option("--oversample <oversample_rate>", "Oversampling rate for recorded data", 1);
	fprintf(stderr, "%*s(sampling rate will be set to %u * oversample_rate)\n", USAGE_OPT_NAME_COLWIDTH, "", SYMBOL_RATE * SPS);
	fprintf(stderr, "%*sDefault: %u\n", USAGE_OPT_NAME_COLWIDTH, "", FILE_OVERSAMPLE);

	describe_option("--sample-format <sample_format>", "Input sample format. Supported formats:", 1);
	describe_option("U8", "8-bit unsigned (eg. recorded with rtl_sdr) (default)", 2);
	describe_option("S16LE", "16-bit signed, little-endian (eg. recorded with miri_sdr)", 2);

	fprintf(stderr, "\nOutput options:\n");
	describe_option("--output <output_specifier>", "Output specification (default: " DEFAULT_OUTPUT ")", 1);
	describe_option("", "(See \"--output help\" for details)", 1);
	describe_option("--output-queue-hwm <integer>", "High water mark value for output queues (0 = no limit)", 1);
	fprintf(stderr, "%*s(default: %d messages, not applicable when using --iq-file or --raw-frames-file)\n", USAGE_OPT_NAME_COLWIDTH, "", OUTPUT_QUEUE_HWM_DEFAULT);
	describe_option("--decode-fragments", "Decode higher level protocols in fragmented packets", 1);
	describe_option("--gs-file <file>", "Read ground station info from <file> (MultiPSK format)", 1);
#ifdef WITH_SQLITE
	describe_option("--bs-db <file>", "Read aircraft info from Basestation database <file> (SQLite)", 1);
#endif
	describe_option("--addrinfo terse|normal|verbose", "Aircraft/ground station info verbosity level (default: normal)", 1);
	describe_option("--station-id <name>", "Receiver site identifier", 1);
	fprintf(stderr, "%*sMaximum length: %u characters\n", USAGE_OPT_NAME_COLWIDTH, "", STATION_ID_LEN_MAX);
	describe_option("--msg-filter <filter_spec>", "Output only a specified subset of messages (default: all)", 1);
	describe_option("", "(See \"--msg-filter help\" for details)", 1);
#ifdef WITH_STATSD
	describe_option("--statsd <host>:<port>", "Send statistics to Etsy StatsD server <host>:<port>", 1);
#endif

	fprintf(stderr, "\nText output formatting options:\n");
	describe_option("--utc", "Use UTC timestamps in output and file names", 1);
	describe_option("--milliseconds", "Print milliseconds in timestamps", 1);
	describe_option("--raw-frames", "Print raw AVLC frame as hex", 1);
	describe_option("--dump-asn1", "Print full ASN.1 structure of CM and CPDLC messages", 1);
	describe_option("--extended-header", "Print additional fields in message header", 1);
	describe_option("--prettify-xml", "Pretty-print XML payloads in ACARS and MIAM CORE PDUs", 1);
	_exit(0);
}

void print_msg_filterspec_list(msg_filterspec_t const *filters) {
	for(msg_filterspec_t const *ptr = filters; ptr->token != NULL; ptr++) {
		describe_option(ptr->token, ptr->description, 2);
	}
}

static msg_filterspec_t const msg_filters[] = {
	{ "all",                MSGFLT_ALL,                     "All messages" },
	{ "uplink",             MSGFLT_SRC_GND,                 "Uplink messages (sourced by ground stations)" },
	{ "downlink",           MSGFLT_SRC_AIR,                 "Downlink messages (sourced by aircraft)" },
	{ "avlc_s",             MSGFLT_AVLC_S,                  "AVLC Supervisory frames" },
	{ "avlc_u",             MSGFLT_AVLC_U,                  "AVLC Unnumbered Control frames" },
	{ "avlc_i",             MSGFLT_AVLC_I,                  "AVLC Information frames" },
	{ "avlc",               MSGFLT_AVLC_S | MSGFLT_AVLC_U | MSGFLT_AVLC_I, "All AVLC frames (shorthand for \"avlc_s,avlc_u,avlc_i)\"", },
	{ "acars_nodata",       MSGFLT_ACARS_NODATA,            "ACARS frames without data (eg. empty ACKs)" },
	{ "acars_data",         MSGFLT_ACARS_DATA,              "ACARS frames with data" },
	{ "acars",              MSGFLT_ACARS_NODATA | MSGFLT_ACARS_DATA, "All ACARS frames (shorthand for \"acars_nodata,acars_data\")" },
	{ "xid_no_gsif",        MSGFLT_XID_NO_GSIF,             "XID frames other than Ground Station Information Frames" },
	{ "gsif",               MSGFLT_XID_GSIF,                "Ground Station Information Frames" },
	{ "xid",                MSGFLT_XID_NO_GSIF | MSGFLT_XID_GSIF, "All XID frames (shorthand for \"xid_no_gsif,gsif\")" },
	{ "x25_control",        MSGFLT_X25_CONTROL,             "X.25 Control packets" },
	{ "x25_data",           MSGFLT_X25_DATA,                "X.25 Data packets" },
	{ "x25",                MSGFLT_X25_CONTROL | MSGFLT_X25_DATA, "All X.25 packets (shorthand for \"x25_control,x25_data\")" },
	{ "idrp_no_keepalive",  MSGFLT_IDRP_NO_KEEPALIVE,       "IDRP PDUs other than Keepalives" },
	{ "idrp_keepalive",     MSGFLT_IDRP_KEEPALIVE,          "IDRP Keepalive PDUs" },
	{ "idrp",               MSGFLT_IDRP_NO_KEEPALIVE | MSGFLT_IDRP_KEEPALIVE, "All IDRP PDUs (shorthand for \"idrp_no_keepalive,idrp_keepalive\")" },
	{ "esis",               MSGFLT_ESIS,                    "ES-IS PDUs" },
	{ "cm",                 MSGFLT_CM,                      "ICAO Context Management Protocol PDUs" },
	{ "cpdlc",              MSGFLT_CPDLC,                   "Controller-Pilot Data Link Communication PDUs" },
	{ "adsc",               MSGFLT_ADSC,                    "Automatic Dependent Surveillance - Contract messages" },
	{ 0,                    0,                              0 }
};

#ifdef DEBUG
static msg_filterspec_t const debug_filters[] = {
	{ "none",               D_NONE,                         "No messages" },
	{ "all",                D_ALL,                          "All messages" },
	{ "sdr",                D_SDR,                          "SDR device handling" },
	{ "demod",              D_DEMOD,                        "DSP and demodulation" },
	{ "demod_detail",       D_DEMOD_DETAIL,                 "DSP and demodulation - details with raw data dumps" },
	{ "burst",              D_BURST,                        "VDL2 burst decoding" },
	{ "burst_detail",       D_BURST_DETAIL,                 "VDL2 burst decoding - details with raw data dumps" },
	{ "proto",              D_PROTO,                        "Frame payload decoding" },
	{ "proto_detail",       D_PROTO_DETAIL,                 "Frame payload decoding - details with raw data dumps" },
	{ "stats",              D_STATS,                        "Statistics generation" },
	{ "cache",              D_CACHE,                        "AC and GS data cache operations" },
	{ "output",             D_OUTPUT,                       "Data output operations" },
	{ "misc",               D_MISC,                         "Messages not falling into other categories" },
	{ 0,                    0,                              0 }
};

static void debug_filter_usage() {
	fprintf(stderr,
			"<filter_spec> is a comma-separated list of words specifying debug classes which should\n"
			"be printed.\n\nSupported debug classes:\n\n"
		   );

	print_msg_filterspec_list(debug_filters);

	fprintf(stderr,
			"\nBy default, no debug messages are printed.\n"
		   );
}
#endif

static void msg_filter_usage() {
	fprintf(stderr,
			"<filter_spec> is a comma-separated list of words specifying message types which should\n"
			"be displayed. Each word may optionally be preceded by a '-' sign to negate its meaning\n"
			"(ie. to indicate that a particular message type shall not be displayed).\n"
			"\nSupported message types:\n\n"
		   );

	print_msg_filterspec_list(msg_filters);

	fprintf(stderr,
			"\nWhen --msg-filter option is not used, all messages are displayed. But when it is, the\n"
			"filter is first reset to \"none\", ie. you have to explicitly enable all message types\n"
			"which you wish to see. Word list is parsed from left to right, so the last match wins.\n"
			"\nRefer to FILTERING_EXAMPLES.md file for usage examples.\n"
		   );
}

static void update_filtermask(msg_filterspec_t const *filters, char *token, uint32_t *fmask) {
	bool negate = false;
	if(token[0] == '-') {
		negate = true;
		token++;
		if(token[0] == '\0') {
			fprintf(stderr, "Invalid filtermask: no token after '-'\n");
			_exit(1);
		}
	}
	for(msg_filterspec_t const *ptr = filters; ptr->token != NULL; ptr++) {
		if(!strcmp(token, ptr->token)) {
			if(negate)
				*fmask &= ~ptr->value;
			else
				*fmask |= ptr->value;
			return;
		}
	}
	fprintf(stderr, "Unknown filter specifier: %s\n", token);
	_exit(1);
}

static uint32_t parse_msg_filterspec(msg_filterspec_t const *filters, void (*help)(), char *filterspec) {
	if(!strcmp(filterspec, "help")) {
		help();
		_exit(0);
	}
	uint32_t fmask = 0;
	char *token = strtok(filterspec, ",");
	if(token == NULL) {
		fprintf(stderr, "Invalid filter specification\n");
		_exit(1);
	}
	update_filtermask(filters, token, &fmask);
	while((token = strtok(NULL, ",")) != NULL) {
		update_filtermask(filters, token, &fmask);
	}
	return fmask;
}

int main(int argc, char **argv) {
	vdl2_state_t ctx;
	uint32_t centerfreq = 0, sample_rate = 0, oversample = 0;
	uint32_t *freqs;
	int num_channels = 0;
	enum input_types input = INPUT_UNDEF;
	enum sample_formats sample_fmt = SFMT_UNDEF;
	la_list *fmtr_list = NULL;
	bool input_is_iq = true;
	pthread_t decoder_thread;
#if defined WITH_RTLSDR || defined WITH_MIRISDR || defined WITH_SDRPLAY || defined WITH_SDRPLAY3 || defined WITH_SOAPYSDR
	char *device = NULL;
	float gain = SDR_AUTO_GAIN;
	int correction = 0;
#endif
#ifdef WITH_MIRISDR
	int mirisdr_hw_flavour = 0;
	int mirisdr_usb_xfer_mode = 0;
#endif
#if defined WITH_SDRPLAY || defined WITH_SDRPLAY3
	char *sdrplay_antenna = NULL;
	int sdrplay_biast = 0;
	int sdrplay_notch_filter = 0;
	int sdrplay_tuner = 1;
	int sdrplay_agc = 0;
#endif
#ifdef WITH_SDRPLAY
	int sdrplay_gr = SDR_AUTO_GAIN;
#endif
#ifdef WITH_SDRPLAY3
	int sdrplay3_dab_notch_filter = 0;
	int sdrplay3_ifgr = SDR_AUTO_GAIN;
	int sdrplay3_lna_state = SDR_AUTO_GAIN;
#endif
#ifdef WITH_SOAPYSDR
	char *soapysdr_settings = NULL;
	char *soapysdr_antenna = NULL;
	char *soapysdr_gain = NULL;
#endif
	int opt;
	struct option long_opts[] = {
		{ "centerfreq",         required_argument,  NULL,   __OPT_CENTERFREQ },
		{ "station-id",         required_argument,  NULL,   __OPT_STATION_ID },
		{ "utc",                no_argument,        NULL,   __OPT_UTC },
		{ "milliseconds",       no_argument,        NULL,   __OPT_MILLISECONDS },
		{ "raw-frames",         no_argument,        NULL,   __OPT_RAW_FRAMES },
		{ "dump-asn1",          no_argument,        NULL,   __OPT_DUMP_ASN1 },
		{ "extended-header",    no_argument,        NULL,   __OPT_EXTENDED_HEADER },
		{ "decode-fragments",   no_argument,        NULL,   __OPT_DECODE_FRAGMENTS },
		{ "prettify-xml",       no_argument,        NULL,   __OPT_PRETTIFY_XML },
		{ "gs-file",            required_argument,  NULL,   __OPT_GS_FILE },
#ifdef WITH_SQLITE
		{ "bs-db",              required_argument,  NULL,   __OPT_BS_DB },
#endif
		{ "addrinfo",           required_argument,  NULL,   __OPT_ADDRINFO_VERBOSITY },
		{ "output",             required_argument,  NULL,   __OPT_OUTPUT },
		{ "output-queue-hwm",   required_argument,  NULL,   __OPT_OUTPUT_QUEUE_HWM },
		{ "iq-file",            required_argument,  NULL,   __OPT_IQ_FILE },
		{ "oversample",         required_argument,  NULL,   __OPT_OVERSAMPLE },
		{ "sample-format",      required_argument,  NULL,   __OPT_SAMPLE_FORMAT },
		{ "msg-filter",         required_argument,  NULL,   __OPT_MSG_FILTER },
#ifdef WITH_MIRISDR
		{ "mirisdr",            required_argument,  NULL,   __OPT_MIRISDR },
		{ "hw-type",            required_argument,  NULL,   __OPT_HW_TYPE },
		{ "usb-mode",           required_argument,  NULL,   __OPT_USB_MODE },
#endif
#ifdef WITH_SDRPLAY
		{ "sdrplay",            required_argument,  NULL,   __OPT_SDRPLAY },
		{ "gr",                 required_argument,  NULL,   __OPT_GR },
#endif
#ifdef WITH_SDRPLAY3
		{ "sdrplay3",           required_argument,  NULL,   __OPT_SDRPLAY3 },
		{ "ifgr",               required_argument,  NULL,   __OPT_SDRPLAY3_IFGR },
		{ "lna-state",          required_argument,  NULL,   __OPT_SDRPLAY3_LNA_STATE },
		{ "dab-notch-filter",   required_argument,  NULL,   __OPT_SDRPLAY3_DAB_NOTCH_FILTER },
#endif
#if defined WITH_SDRPLAY || defined WITH_SDRPLAY3
		{ "antenna",            required_argument,  NULL,   __OPT_ANTENNA },
		{ "biast",              required_argument,  NULL,   __OPT_BIAST },
		{ "notch-filter",       required_argument,  NULL,   __OPT_NOTCH_FILTER },
		{ "agc",                required_argument,  NULL,   __OPT_AGC },
		{ "tuner",              required_argument,  NULL,   __OPT_TUNER },
#endif
#ifdef WITH_SOAPYSDR
		{ "soapysdr",           required_argument,  NULL,   __OPT_SOAPYSDR },
		{ "device-settings",    required_argument,  NULL,   __OPT_DEVICE_SETTINGS },
		{ "soapy-antenna",      required_argument,  NULL,   __OPT_SOAPY_ANTENNA },
		{ "soapy-gain",         required_argument,  NULL,   __OPT_SOAPY_GAIN },
#endif
#ifdef WITH_RTLSDR
		{ "rtlsdr",             required_argument,  NULL,   __OPT_RTLSDR },
#endif
#if defined WITH_RTLSDR || defined WITH_MIRISDR || defined WITH_SOAPYSDR
		{ "gain",               required_argument,  NULL,   __OPT_GAIN },
#endif
#if defined WITH_RTLSDR || defined WITH_MIRISDR || defined WITH_SDRPLAY || defined WITH_SDRPLAY3 || defined WITH_SOAPYSDR
		{ "correction",         required_argument,  NULL,   __OPT_CORRECTION },
#endif
#ifdef WITH_PROTOBUF_C
		{ "raw-frames-file",    required_argument,  NULL,   __OPT_RAW_FRAMES_FILE },
#endif
#ifdef WITH_STATSD
		{ "statsd",             required_argument,  NULL,   __OPT_STATSD },
#endif
		{ "version",            no_argument,        NULL,   __OPT_VERSION },
		{ "help",               no_argument,        NULL,   __OPT_HELP },
#ifdef DEBUG
		{ "debug",              required_argument,  NULL,   __OPT_DEBUG },
#endif
		{ 0,                    0,                  0,      0 }
	};

#ifdef WITH_STATSD
	char *statsd_addr = NULL;
	bool statsd_enabled = false;
#endif
#ifdef WITH_SQLITE
	char *bs_db_file = NULL;
#endif
	char *infile = NULL;
	char *gs_file = NULL;

	// Initialize default config
	memset(&Config, 0, sizeof(Config));
	Config.addrinfo_verbosity = ADDRINFO_NORMAL;
	Config.msg_filter = MSGFLT_ALL;
	Config.output_queue_hwm = OUTPUT_QUEUE_HWM_DEFAULT;

	print_version();
	while((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
		switch(opt) {
#ifdef WITH_PROTOBUF_C
			case __OPT_RAW_FRAMES_FILE:
				infile = strdup(optarg);
				input = INPUT_RAW_FRAMES_FILE;
				input_is_iq = false;
				break;
#endif
			case __OPT_IQ_FILE:
				infile = strdup(optarg);
				input = INPUT_IQ_FILE;
				oversample = FILE_OVERSAMPLE;
				sample_fmt = SFMT_U8;
				break;
			case __OPT_SAMPLE_FORMAT:
				if(!strcmp(optarg, "U8"))
					sample_fmt = SFMT_U8;
				else if(!strcmp(optarg, "S16_LE"))
					sample_fmt = SFMT_S16_LE;
				else {
					fprintf(stderr, "Unknown sample format\n");
					_exit(1);
				}
				break;
			case __OPT_UTC:
				Config.utc = true;
				break;
			case __OPT_MILLISECONDS:
				Config.milliseconds = true;
				break;
			case __OPT_RAW_FRAMES:
				Config.output_raw_frames = true;
				break;
			case __OPT_DUMP_ASN1:
				Config.dump_asn1 = true;
				la_config_set_bool("dump_asn1", true);
				break;
			case __OPT_EXTENDED_HEADER:
				Config.extended_header = true;
				break;
			case __OPT_DECODE_FRAGMENTS:
				Config.decode_fragments = true;
				la_config_set_bool("decode_fragments", true);
				break;
			case __OPT_PRETTIFY_XML:
				la_config_set_bool("prettify_xml", true);
				break;
			case __OPT_GS_FILE:
				gs_file = optarg;
				break;
#ifdef WITH_SQLITE
			case __OPT_BS_DB:
				bs_db_file = optarg;
				break;
#endif
			case __OPT_ADDRINFO_VERBOSITY:
				if(!strcmp(optarg, "terse")) {
					Config.addrinfo_verbosity = ADDRINFO_TERSE;
				} else if(!strcmp(optarg, "normal")) {
					Config.addrinfo_verbosity = ADDRINFO_NORMAL;
				} else if(!strcmp(optarg, "verbose")) {
					Config.addrinfo_verbosity = ADDRINFO_VERBOSE;
				} else {
					fprintf(stderr, "Invalid value for option --addrinfo\n");
					fprintf(stderr, "Use --help for help\n");
					_exit(1);
				}
				break;
			case __OPT_STATION_ID:
				if(strlen(optarg) > STATION_ID_LEN_MAX) {
					fprintf(stderr, "Warning: station-id value too long; truncated to %d characters\n",
							STATION_ID_LEN_MAX);
				}
				Config.station_id = strndup(optarg, STATION_ID_LEN_MAX);
				break;
			case __OPT_CENTERFREQ:
				centerfreq = strtoul(optarg, NULL, 10);
				break;
#ifdef WITH_MIRISDR
			case __OPT_MIRISDR:
				device = optarg;
				input = INPUT_MIRISDR;
				oversample = MIRISDR_OVERSAMPLE;
				break;
			case __OPT_HW_TYPE:
				mirisdr_hw_flavour = atoi(optarg);
				break;
			case __OPT_USB_MODE:
				mirisdr_usb_xfer_mode = atoi(optarg);
				break;
#endif
#ifdef WITH_SDRPLAY
			case __OPT_SDRPLAY:
				device = optarg;
				input = INPUT_SDRPLAY;
				oversample = SDRPLAY_OVERSAMPLE;
				break;
			case __OPT_GR:
				sdrplay_gr = atoi(optarg);
				break;
#endif
#ifdef WITH_SDRPLAY3
			case __OPT_SDRPLAY3:
				device = optarg;
				input = INPUT_SDRPLAY3;
				oversample = SDRPLAY3_OVERSAMPLE;
				break;
			case __OPT_SDRPLAY3_IFGR:
				sdrplay3_ifgr = atoi(optarg);
				break;
			case __OPT_SDRPLAY3_LNA_STATE:
				sdrplay3_lna_state = atoi(optarg);
				break;
			case __OPT_SDRPLAY3_DAB_NOTCH_FILTER:
				sdrplay3_dab_notch_filter = atoi(optarg);
				break;
#endif
#if defined WITH_SDRPLAY || defined WITH_SDRPLAY3
			case __OPT_ANTENNA:
				sdrplay_antenna = strdup(optarg);
				break;
			case __OPT_BIAST:
				sdrplay_biast = atoi(optarg);
				break;
			case __OPT_NOTCH_FILTER:
				sdrplay_notch_filter = atoi(optarg);
				break;
			case __OPT_AGC:
				sdrplay_agc = atoi(optarg);
				break;
			case __OPT_TUNER:
				sdrplay_tuner = atoi(optarg);
				break;
#endif
#ifdef WITH_SOAPYSDR
			case __OPT_SOAPYSDR:
				device = optarg;
				input = INPUT_SOAPYSDR;
				oversample = SOAPYSDR_OVERSAMPLE;
				break;
			case __OPT_DEVICE_SETTINGS:
				soapysdr_settings = strdup(optarg);
				break;
			case __OPT_SOAPY_ANTENNA:
				soapysdr_antenna = strdup(optarg);
				break;
			case __OPT_SOAPY_GAIN:
				soapysdr_gain = strdup(optarg);
				break;
#endif
#ifdef WITH_RTLSDR
			case __OPT_RTLSDR:
				device = optarg;
				input = INPUT_RTLSDR;
				oversample = RTL_OVERSAMPLE;
				break;
#endif
#if defined WITH_RTLSDR || defined WITH_MIRISDR || defined WITH_SOAPYSDR
			case __OPT_GAIN:
				gain = atof(optarg);
				break;
#endif
#if defined WITH_RTLSDR || defined WITH_MIRISDR || defined WITH_SDRPLAY || defined WITH_SDRPLAY3 || defined WITH_SOAPYSDR
			case __OPT_CORRECTION:
				correction = atoi(optarg);
				break;
#endif
			case __OPT_OUTPUT:
				fmtr_list = setup_output(fmtr_list, optarg);
				break;
			case __OPT_OUTPUT_QUEUE_HWM:
				Config.output_queue_hwm = atoi(optarg);
				if(Config.output_queue_hwm < 0) {
					fprintf(stderr, "Invalid --output-queue-hwm value: must be a non-negative integer\n");
					_exit(1);
				}
				break;
			case __OPT_OVERSAMPLE:
				oversample = atoi(optarg);
				break;
#ifdef WITH_STATSD
			case __OPT_STATSD:
				statsd_addr = strdup(optarg);
				statsd_enabled = true;
				break;
#endif
			case __OPT_MSG_FILTER:
				Config.msg_filter = parse_msg_filterspec(msg_filters, msg_filter_usage, optarg);
				break;
#ifdef DEBUG
			case __OPT_DEBUG:
				Config.debug_filter = parse_msg_filterspec(debug_filters, debug_filter_usage, optarg);
				debug_print(D_MISC, "debug filtermask: 0x%x\n", Config.debug_filter);
				break;
#endif
			case __OPT_VERSION:
				// No-op - the version has been printed before getopt().
				_exit(0);
			case __OPT_HELP:
			default:
				usage();
		}
	}
	if(input == INPUT_UNDEF) {
		fprintf(stderr, "No input specified\n");
		fprintf(stderr, "Use --help for help\n");
		_exit(1);
	}

// no --output given?
	if(fmtr_list == NULL) {
		fmtr_list = setup_output(fmtr_list, DEFAULT_OUTPUT);
	}
	ASSERT(fmtr_list != NULL);

	if(input_is_iq) {
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
	}

	if(gs_file != NULL) {
		if(gs_data_import(gs_file) < 0) {
			fprintf(stderr, "Failed to import ground station data file. "
					"Extended data for ground stations will not be logged.\n");
		} else {
			Config.gs_addrinfo_db_available = true;
		}
	}
#ifdef WITH_STATSD
	if(statsd_enabled) {
		if(statsd_initialize(statsd_addr) < 0) {
			fprintf(stderr, "Failed to initialize statsd client - disabling\n");
			XFREE(statsd_addr);
			statsd_enabled = false;
		} else {
			if(input_is_iq) {
				for(int i = 0; i < num_channels; i++) {
					statsd_initialize_counters_per_channel(freqs[i]);
				}
			}
			statsd_initialize_counters_per_msgdir();
		}
	} else {
		XFREE(statsd_addr);
		statsd_enabled = false;
	}
#endif
#ifdef WITH_SQLITE
	if(bs_db_file != NULL) {
		if(ac_data_init(bs_db_file) < 0) {
			fprintf(stderr, "Failed to open aircraft database. "
					"Extended data for aircraft will not be logged.\n");
		} else {
			Config.ac_addrinfo_db_available = true;
		}
	}
#endif

	// Configure libacars
	la_config_set_int("acars_bearer", LA_ACARS_BEARER_VHF);

	setup_signals();
	start_all_output_threads(fmtr_list);
	avlc_decoder_init();
	start_thread(&decoder_thread, avlc_decoder_thread, fmtr_list);

	if(input_is_iq) {
		sincosf_lut_init();
		input_lpf_init(sample_rate);
		demod_sync_init();
		setup_barriers(&ctx);
		start_demod_threads(&ctx);
	}

#ifdef WITH_PROFILING
    ProfilerStart("dumpvdl2.prof");
#endif
	int exit_code = 0;
	switch(input) {
#ifdef WITH_PROTOBUF_C
		case INPUT_RAW_FRAMES_FILE:
			Config.output_queue_hwm = OUTPUT_QUEUE_HWM_NONE;
			exit_code = input_raw_frames_file_process(infile);
			break;
#endif
		case INPUT_IQ_FILE:
			Config.output_queue_hwm = OUTPUT_QUEUE_HWM_NONE;
			process_iq_file(&ctx, infile, sample_fmt);
			pthread_barrier_wait(&demods_ready);
			break;
#ifdef WITH_RTLSDR
		case INPUT_RTLSDR:
			rtl_init(&ctx, device, centerfreq, gain, correction);
			break;
#endif
#ifdef WITH_MIRISDR
		case INPUT_MIRISDR:
			mirisdr_init(&ctx, device, mirisdr_hw_flavour, centerfreq, gain, correction, mirisdr_usb_xfer_mode);
			break;
#endif
#ifdef WITH_SDRPLAY
		case INPUT_SDRPLAY:
			sdrplay_init(&ctx, device, sdrplay_antenna, centerfreq, sdrplay_gr, correction,
					sdrplay_biast, sdrplay_notch_filter, sdrplay_agc, sdrplay_tuner);
			break;
#endif
#ifdef WITH_SDRPLAY3
		case INPUT_SDRPLAY3:
			sdrplay3_init(&ctx, device, sdrplay_antenna, centerfreq, sdrplay3_ifgr, sdrplay3_lna_state, correction,
					sdrplay_biast, sdrplay_notch_filter, sdrplay3_dab_notch_filter, sdrplay_agc, sdrplay_tuner);
			break;
#endif
#ifdef WITH_SOAPYSDR
		case INPUT_SOAPYSDR:
			soapysdr_init(&ctx, device, soapysdr_antenna, centerfreq, gain, correction,
					soapysdr_settings, soapysdr_gain);
			break;
#endif
		default:
			fprintf(stderr, "Unknown input type\n");
			exit_code = 5;
			break;
	}
	avlc_decoder_shutdown();

	fprintf(stderr, "Waiting for output threads to finish\n");
	int active_threads_cnt = 0;
	do {
		active_threads_cnt = 0;
		usleep(500000);
		if(decoder_thread_active) {
			active_threads_cnt++;
		}
		fmtr_instance_t *fmtr = NULL;
		for(la_list *fl = fmtr_list; fl != NULL; fl = la_list_next(fl)) {
			fmtr = (fmtr_instance_t *)(fl->data);
			output_instance_t *output = NULL;
			for(la_list *ol = fmtr->outputs; ol != NULL; ol = la_list_next(ol)) {
				output = (output_instance_t *)(ol->data);
				if(output->ctx->active) {
					active_threads_cnt++;
				}
			}
		}
	} while(active_threads_cnt != 0 && do_exit < 2);
	fprintf(stderr, "Exiting\n");
#ifdef WITH_PROFILING
    ProfilerStop();
#endif
	return(exit_code);
}
