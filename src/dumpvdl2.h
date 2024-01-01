/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2023 Tomasz Lemiech <szpajder@gmail.com>
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
#ifndef _DUMPVDL2_H
#define _DUMPVDL2_H 1
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>             // abort()
#include <sys/time.h>
#include <pthread.h>            // pthread_t, pthread_barrier_t
#include <libacars/libacars.h>  // la_proto_node
#include <libacars/vstring.h>   // la_vstring
#include <libacars/dict.h>      // la_dict
#include "config.h"
#ifndef HAVE_PTHREAD_BARRIERS
#include "pthread_barrier.h"
#endif

#define RS_K 249                // Reed-Solomon vector length (bytes)
#define RS_N 255                // Reed-Solomon codeword length (bytes)
#define TRLEN 17                // transmission length field length (bits)
#define HDRFECLEN 5             // CRC field length (bits)
#define HEADER_LEN (3 + TRLEN + HDRFECLEN)
#define PREAMBLE_SYMS 16
#define SYNC_BUFLEN (PREAMBLE_SYMS * SPS)    // length of look-behind buffer used for frame syncing
#define SPS 10
#define BPS 3
#define SYMBOL_RATE 10500
#define CSC_FREQ 136975000U
#define FILE_BUFSIZE 320000U
#define FILE_OVERSAMPLE 10
#define SDR_AUTO_GAIN -100.0f

// long command line options
#define __OPT_CENTERFREQ              1
#define __OPT_STATION_ID              2
#ifdef WITH_PROTOBUF_C
#define __OPT_RAW_FRAMES_FILE         3
#endif
#define __OPT_OUTPUT                  4
#define __OPT_IQ_FILE                 5
#define __OPT_OVERSAMPLE              6
#define __OPT_SAMPLE_FORMAT           7

#ifdef WITH_MIRISDR
#define __OPT_MIRISDR                 8
#define __OPT_HW_TYPE                 9
#define __OPT_USB_MODE               10
#endif

#ifdef WITH_RTLSDR
#define __OPT_RTLSDR                 11
#endif

#if defined WITH_MIRISDR || defined WITH_RTLSDR || defined WITH_SOAPYSDR
#define __OPT_GAIN                   12
#endif

#if defined WITH_MIRISDR || defined WITH_RTLSDR || defined WITH_SDRPLAY || defined WITH_SDRPLAY3 || defined WITH_SOAPYSDR
#define __OPT_CORRECTION             13
#endif

#ifdef WITH_STATSD
#define __OPT_STATSD                 14
#endif
#define __OPT_MSG_FILTER             15
#define __OPT_OUTPUT_QUEUE_HWM       16
#define __OPT_UTC                    17
#define __OPT_RAW_FRAMES             18
#define __OPT_DUMP_ASN1              19
#define __OPT_EXTENDED_HEADER        20
#define __OPT_DECODE_FRAGMENTS       21
#define __OPT_GS_FILE                22
#ifdef WITH_SQLITE
#define __OPT_BS_DB                  23
#endif
#define __OPT_ADDRINFO_VERBOSITY     24
#define __OPT_PRETTIFY_XML           25
#define __OPT_MILLISECONDS           26
#define __OPT_PRETTIFY_JSON          27

#ifdef WITH_SDRPLAY3
#define __OPT_SDRPLAY3               70
#define __OPT_SDRPLAY3_DAB_NOTCH_FILTER      71
#define __OPT_SDRPLAY3_IFGR          72
#define __OPT_SDRPLAY3_LNA_STATE     73
#endif

#ifdef WITH_SDRPLAY
#define __OPT_SDRPLAY                80
#define __OPT_GR                     86
#endif
#if defined WITH_SDRPLAY || defined WITH_SDRPLAY3
#define __OPT_ANTENNA                81
#define __OPT_BIAST                  82
#define __OPT_NOTCH_FILTER           83
#define __OPT_AGC                    84
#define __OPT_TUNER                  85
#endif

#ifdef WITH_SOAPYSDR
#define __OPT_SOAPYSDR               90
#define __OPT_DEVICE_SETTINGS        91
#define __OPT_SOAPY_ANTENNA          92
#define __OPT_SOAPY_GAIN             93
#endif

#define __OPT_VERSION                98
#define __OPT_HELP                   99
#ifdef DEBUG
#define __OPT_DEBUG                 100
#endif

// message filters
#define MSGFLT_ALL                  (~0)
#define MSGFLT_NONE                 (0)
#define MSGFLT_SRC_GND              (1 <<  0)
#define MSGFLT_SRC_AIR              (1 <<  1)
#define MSGFLT_AVLC_S               (1 <<  2)
#define MSGFLT_AVLC_U               (1 <<  3)
#define MSGFLT_AVLC_I               (1 <<  4)
#define MSGFLT_ACARS_NODATA         (1 <<  5)
#define MSGFLT_ACARS_DATA           (1 <<  6)
#define MSGFLT_XID_NO_GSIF          (1 <<  7)
#define MSGFLT_XID_GSIF             (1 <<  8)
#define MSGFLT_X25_CONTROL          (1 <<  9)
#define MSGFLT_X25_DATA             (1 << 10)
#define MSGFLT_IDRP_NO_KEEPALIVE    (1 << 11)
#define MSGFLT_IDRP_KEEPALIVE       (1 << 12)
#define MSGFLT_ESIS                 (1 << 13)
#define MSGFLT_CM                   (1 << 14)
#define MSGFLT_CPDLC                (1 << 15)
#define MSGFLT_ADSC                 (1 << 16)

// debug message classes
#define D_ALL                       (~0)
#define D_NONE                      (0)
#define D_SDR                       (1 <<  0)
#define D_DEMOD                     (1 <<  1)
#define D_DEMOD_DETAIL              (1 <<  2)
#define D_BURST                     (1 <<  3)
#define D_BURST_DETAIL              (1 <<  4)
#define D_PROTO                     (1 <<  5)
#define D_PROTO_DETAIL              (1 <<  6)
#define D_STATS                     (1 <<  7)
#define D_CACHE                     (1 <<  8)
#define D_OUTPUT                    (1 <<  9)
#define D_MISC                      (1 << 31)

// default output specification - decoded text output to stdout
#define DEFAULT_OUTPUT "decoded:text:file:path=-"

// output queue high water mark
#define OUTPUT_QUEUE_HWM_DEFAULT 1000
// high water mark disabled
#define OUTPUT_QUEUE_HWM_NONE 0

// help text pretty-printing constants and macros
#define USAGE_INDENT_STEP 4
#define USAGE_OPT_NAME_COLWIDTH 48
#define IND(n) (n * USAGE_INDENT_STEP)

// option name and description to be printed in the help text
typedef struct{
	char *name;
	char *description;
} option_descr_t;

// maximum length of station_id parameter
#define STATION_ID_LEN_MAX 255

typedef struct {
	char *token;
	uint32_t value;
	char *description;
} msg_filterspec_t;

typedef enum {
	ADDRINFO_TERSE = 0,
	ADDRINFO_NORMAL = 1,
	ADDRINFO_VERBOSE = 2
} addrinfo_verbosity_t;

// global config
typedef struct {
#ifdef DEBUG
	uint32_t debug_filter;
#endif
	uint32_t msg_filter;
	int output_queue_hwm;
	char *station_id;
	bool hourly, daily, utc, milliseconds;
	bool output_raw_frames, dump_asn1, extended_header, decode_fragments;
	bool ac_addrinfo_db_available;
	bool gs_addrinfo_db_available;
	addrinfo_verbosity_t addrinfo_verbosity;
} dumpvdl2_config_t;

#define nop() do {} while (0)

#ifdef __GNUC__
#define LIKELY(x)   (__builtin_expect(!!(x),1))
#define UNLIKELY(x) (__builtin_expect(!!(x),0))
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

#ifdef __GNUC__
#define PRETTY_FUNCTION __PRETTY_FUNCTION__
#else
#define PRETTY_FUNCTION ""
#endif

#define ASSERT_se(expr) \
	do { \
		if (UNLIKELY(!(expr))) { \
			fprintf(stderr, "Assertion '%s' failed at %s:%u, function %s(). Aborting.\n", #expr, \
					__FILE__, __LINE__, PRETTY_FUNCTION); \
				abort(); \
		} \
	} while (0)

#ifdef NDEBUG
#define ASSERT(expr) nop()
#else
#define ASSERT(expr) ASSERT_se(expr)
#endif

#ifdef DEBUG
#define debug_print(debug_class, fmt, ...) \
				do { \
					if(Config.debug_filter & debug_class) { \
						fprintf(stderr, "%s(): " fmt, __func__, ##__VA_ARGS__); \
					} \
				} while (0)

#define debug_print_buf_hex(debug_class, buf, len, fmt, ...) \
				do { \
					if(Config.debug_filter & debug_class) { \
						fprintf(stderr, "%s(): " fmt, __func__, ##__VA_ARGS__); \
						fprintf(stderr, "%s(): ", __func__); \
						for(size_t zz = 0; zz < (len); zz++) { \
							fprintf(stderr, "%02x ", buf[zz]); \
							if(zz && (zz+1) % 32 == 0) fprintf(stderr, "\n%s(): ", __func__); \
						} \
						fprintf(stderr, "\n"); \
					} \
				} while(0)
#else
#define debug_print(debug_class, fmt, ...) nop()
#define debug_print_buf_hex(debug_class, buf, len, fmt, ...) nop()
#endif

#define ONES(x) ~(~0u << (x))
#define XCALLOC(nmemb, size) xcalloc((nmemb), (size), __FILE__, __LINE__, __func__)
#define XREALLOC(ptr, size) xrealloc((ptr), (size), __FILE__, __LINE__, __func__)
#define XFREE(ptr) do { free(ptr); ptr = NULL; } while(0)
#define NEW(type, x) type *(x) = XCALLOC(1, sizeof(type))
#define UNUSED(x) (void)(x)
#define EOL(x) la_vstring_append_sprintf((x), "%s", "\n")
#define SAFE_JSON_APPEND_STRING(v, n, val) \
	do { \
		if((val) != NULL) { \
			la_json_append_string((v), (n), (val)); \
		} \
	} while(0)

typedef struct {
	uint8_t *buf;
	uint32_t start, end, len, descrambler_pos;
} bitstream_t;

enum demod_states { DM_INIT, DM_SYNC };
enum decoder_states { DEC_HEADER, DEC_DATA, DEC_IDLE };
enum input_types {
#ifdef WITH_RTLSDR
	INPUT_RTLSDR,
#endif
#ifdef WITH_MIRISDR
	INPUT_MIRISDR,
#endif
#ifdef WITH_SDRPLAY
	INPUT_SDRPLAY,
#endif
#ifdef WITH_SDRPLAY3
	INPUT_SDRPLAY3,
#endif
#ifdef WITH_SOAPYSDR
	INPUT_SOAPYSDR,
#endif
	INPUT_IQ_FILE,
#ifdef WITH_PROTOBUF_C
	INPUT_RAW_FRAMES_FILE,
#endif
	INPUT_UNDEF
};
enum sample_formats { SFMT_U8, SFMT_S16_LE, SFMT_UNDEF };

typedef struct {
	long long unsigned samplenum;
	bitstream_t *bs, *frame_bs;
	float syncbuf[SYNC_BUFLEN];
	float prev_phi;
	float prev_dphi, dphi;
	float pherr[3];
	float ppm_error;
	float mag_lp;
	float mag_nf;
	float frame_pwr;
	int bufnum;
	int nfcnt;
	int syncbufidx;
	int frame_pwr_cnt;
	int sclk;
	int offset_tuning;
	int num_fec_corrections;
	enum demod_states demod_state;
	enum decoder_states decoder_state;
	uint32_t freq;
	uint32_t downmix_phi, downmix_dphi;
	uint32_t requested_bits;
	uint32_t datalen, datalen_octets, last_block_len_octets, fec_octets;
	uint32_t num_blocks;
	uint32_t syndrome;
	uint16_t lfsr;
	uint16_t oversample;
	struct timeval tstart;
	struct timeval burst_timestamp;
	pthread_t demod_thread;
} vdl2_channel_t;

typedef struct {
	int num_channels;
	vdl2_channel_t **channels;
} vdl2_state_t;

// bitstream.c
bitstream_t *bitstream_init(uint32_t len);
int bitstream_append_msbfirst(bitstream_t *bs, uint8_t const *bytes, uint32_t numbytes, uint32_t numbits);
int bitstream_append_lsbfirst(bitstream_t *bs, uint8_t const *bytes, uint32_t numbytes, uint32_t numbits);
int bitstream_read_lsbfirst(bitstream_t *bs, uint8_t *bytes, uint32_t numbytes, uint32_t numbits);
int bitstream_read_word_msbfirst(bitstream_t *bs, uint32_t *ret, uint32_t numbits);
int bitstream_copy_next_frame(bitstream_t *src, bitstream_t *dst);
void bitstream_descramble(bitstream_t *bs, uint16_t *lfsr);
void bitstream_reset(bitstream_t *bs);
void bitstream_destroy(bitstream_t *bs);
uint32_t reverse(uint32_t v, int numbits);

// demod.c
extern float *sbuf;
vdl2_channel_t *vdl2_channel_init(uint32_t centerfreq, uint32_t freq, uint32_t source_rate, uint32_t oversample);
void sincosf_lut_init();
void input_lpf_init(uint32_t sample_rate);
void demod_sync_init();
void process_buf_uchar_init();
void process_buf_uchar(unsigned char *buf, uint32_t len, void *ctx);
void process_buf_short_init();
void process_buf_short(unsigned char *buf, uint32_t len, void *ctx);
void *process_samples(void *arg);

// crc.c
uint16_t crc16_ccitt(uint8_t *data, uint32_t len, uint16_t crc_init);

// rs.c
int rs_init();
int rs_verify(uint8_t *data, int fec_octets);

// input-raw_frame_file.c
#ifdef WITH_PROTOBUF_C
int input_raw_frames_file_process(char const *file);
#endif

// statsd.c
#ifdef WITH_STATSD
int statsd_initialize(char *statsd_addr);
void statsd_initialize_counters_combined_channels();
void statsd_initialize_counters_per_channel(uint32_t freq);
void statsd_initialize_counters_per_msgdir();
void statsd_initialize_counter_set(char **counter_set);
// Can't have char const * pointers here, because statsd-c-client
// may potentially modify their contents :/
void statsd_counter_per_channel_increment(uint32_t freq, char *counter);
void statsd_timing_delta_per_channel_send(uint32_t freq, char *timer, struct timeval ts);
void statsd_counter_per_msgdir_increment(la_msg_dir msg_dir, char *counter);
void statsd_counter_increment(char *counter);
void statsd_gauge_set(char *gauge, size_t value);
#define statsd_increment_per_channel(freq, counter) statsd_counter_per_channel_increment(freq, counter)
#define statsd_timing_delta_per_channel(freq, timer, start) statsd_timing_delta_per_channel_send(freq, timer, start)
#define statsd_increment_per_msgdir(counter, msgdir) statsd_counter_per_msgdir_increment(counter, msgdir)
#define statsd_increment(counter) statsd_counter_increment(counter)
#define statsd_set(gauge, value) statsd_gauge_set(gauge, value)
#else
#define statsd_increment_per_channel(freq, counter) nop()
#define statsd_timing_delta_per_channel(freq, timer, start) nop()
#define statsd_increment_per_msgdir(counter, msgdir) nop()
#define statsd_increment(counter) nop()
#define statsd_set(gauge, value) nop()
#endif

// util.c
typedef struct {
	uint8_t *buf;
	size_t len;
} octet_string_t;

extern la_type_descriptor const proto_DEF_unknown;
void *xcalloc(size_t nmemb, size_t size, char const *file, int line, char const *func);
void *xrealloc(void *ptr, size_t size, char const *file, int line, char const *func);
uint16_t extract_uint16_msbfirst(uint8_t const *data);
uint32_t extract_uint32_msbfirst(uint8_t const *data);
void bitfield_format_text(la_vstring *vstr, uint8_t const *buf, size_t len, la_dict const *d);
void bitfield_format_json(la_vstring *vstr, uint8_t const *buf, size_t len, la_dict const *d, char const *key);

octet_string_t *octet_string_new(void *buf, size_t len);
octet_string_t *octet_string_copy(octet_string_t const *ostring);
int octet_string_parse(uint8_t *buf, size_t len, octet_string_t *result);
void octet_string_format_text(la_vstring *vstr, octet_string_t const *ostring, int indent);
void octet_string_as_ascii_format_text(la_vstring *vstr, octet_string_t const *ostring, int indent);
void octet_string_as_ascii_format_json(la_vstring *vstr, char const *key,
		octet_string_t const *ostring);
void octet_string_with_ascii_format_text(la_vstring *vstr, octet_string_t const *ostring, int indent);
void octet_string_destroy(octet_string_t *ostring);

char *hexdump(uint8_t *data, size_t len);
void append_hexdump_with_indent(la_vstring *vstr, uint8_t *data, size_t len, int indent);
la_proto_node *unknown_proto_pdu_new(void *buf, size_t len);

// dumpvdl2.c
extern int do_exit;
extern dumpvdl2_config_t Config;
extern pthread_barrier_t demods_ready, samples_ready;
void describe_option(char const *name, char const *description, int indent);

// version.c
extern char const * const DUMPVDL2_VERSION;
#endif // !_DUMPVDL2_H
