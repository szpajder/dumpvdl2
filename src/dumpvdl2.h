/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
 *  Copyright (c) 2017-2018 Tomasz Lemiech <szpajder@gmail.com>
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
#include <stdint.h>
#include <sys/time.h>
#include <pthread.h>		// pthread_t, pthread_barrier_t
#include "config.h"
#include "tlv.h"
#ifndef HAVE_PTHREAD_BARRIERS
#include "pthread_barrier.h"
#endif

#define RS_K 249				// Reed-Solomon vector length (bytes)
#define RS_N 255				// Reed-Solomon codeword length (bytes)
#define TRLEN 17				// transmission length field length (bits)
#define HDRFECLEN 5				// CRC field length (bits)
#define HEADER_LEN (3 + TRLEN + HDRFECLEN)
#define PREAMBLE_SYMS 16
#define SYNC_BUFLEN (PREAMBLE_SYMS * SPS)	// length of look-behind buffer used for frame syncing
#define SPS 10
#define BPS 3
#define SYMBOL_RATE 10500
#define CSC_FREQ 136975000U
#define MAX_CHANNELS 8
#define FILE_BUFSIZE 320000U
#define FILE_OVERSAMPLE 10
#define SDR_AUTO_GAIN -100.0f

// long command line options
#define __OPT_CENTERFREQ		 1
#define __OPT_DAILY			 2
#define __OPT_HOURLY			 3
#define __OPT_OUTPUT_FILE		 4
#define __OPT_IQ_FILE			 5
#define __OPT_OVERSAMPLE		 6
#define __OPT_SAMPLE_FORMAT		 7

#ifdef WITH_MIRISDR
#define __OPT_MIRISDR			 8
#define __OPT_HW_TYPE			 9
#define __OPT_USB_MODE			10
#endif

#ifdef WITH_RTLSDR
#define __OPT_RTLSDR			11
#endif

#if defined WITH_MIRISDR || defined WITH_RTLSDR || defined WITH_SOAPYSDR
#define __OPT_GAIN			12
#endif

#if defined WITH_MIRISDR || defined WITH_RTLSDR || defined WITH_SDRPLAY || defined WITH_SOAPYSDR
#define __OPT_CORRECTION		13
#endif

#ifdef WITH_STATSD
#define __OPT_STATSD			14
#endif
#define __OPT_MSG_FILTER		15
#define __OPT_OUTPUT_ACARS_PP		16
#define __OPT_UTC			17
#define __OPT_RAW_FRAMES		18
#define __OPT_DUMP_ASN1			19
#define __OPT_EXTENDED_HEADER		20

#ifdef WITH_SDRPLAY
#define __OPT_SDRPLAY			80
#define __OPT_ANTENNA			81
#define __OPT_BIAST			82
#define __OPT_NOTCH_FILTER		83
#define __OPT_AGC			84
#define __OPT_GR			85
#define __OPT_TUNER			86
#endif

#ifdef WITH_SOAPYSDR
#define __OPT_SOAPYSDR			90
#define __OPT_DEVICE_SETTINGS		91
#define __OPT_SOAPY_ANTENNA		92
#define __OPT_SOAPY_GAIN		93
#endif

#define __OPT_HELP			99

// message filters
#define MSGFLT_ALL			(~0)
#define MSGFLT_NONE			(0)
#define MSGFLT_SRC_GND			(1 <<  0)
#define MSGFLT_SRC_AIR			(1 <<  1)
#define MSGFLT_AVLC_S			(1 <<  2)
#define MSGFLT_AVLC_U			(1 <<  3)
#define MSGFLT_AVLC_I			(1 <<  4)
#define MSGFLT_ACARS_NODATA		(1 <<  5)
#define MSGFLT_ACARS_DATA		(1 <<  6)
#define MSGFLT_XID_NO_GSIF		(1 <<  7)
#define MSGFLT_XID_GSIF			(1 <<  8)
#define MSGFLT_X25_CONTROL		(1 <<  9)
#define MSGFLT_X25_DATA			(1 << 10)
#define MSGFLT_IDRP_NO_KEEPALIVE	(1 << 11)
#define MSGFLT_IDRP_KEEPALIVE		(1 << 12)
#define MSGFLT_ESIS			(1 << 13)
#define MSGFLT_CM			(1 << 14)
#define MSGFLT_CPDLC			(1 << 15)
#define MSGFLT_ADSC			(1 << 16)

typedef struct {
	char *token;
	uint32_t value;
	char *description;
} msg_filterspec_t;

#define nop() do {} while (0)

#ifdef DEBUG
#define debug_print(fmt, ...) \
	do { fprintf(stderr, "%s(): " fmt, __func__, __VA_ARGS__); } while (0)

#define debug_print_buf_hex(buf, len, fmt, ...) \
	do { \
		fprintf(stderr, "%s(): " fmt, __func__, __VA_ARGS__); \
		fprintf(stderr, "%s(): ", __func__); \
		for(int zz = 0; zz < (len); zz++) { \
			fprintf(stderr, "%02x ", buf[zz]); \
			if(zz && (zz+1) % 32 == 0) fprintf(stderr, "\n%s(): ", __func__); \
		} \
		fprintf(stderr, "\n"); \
	} while(0)
#else
#define debug_print(fmt, ...) nop()
#define debug_print_buf_hex(buf, len, fmt, ...) nop()
#endif

#define ONES(x) ~(~0u << (x))
#define XCALLOC(nmemb, size) xcalloc((nmemb), (size), __FILE__, __LINE__, __func__)
#define XREALLOC(ptr, size) xrealloc((ptr), (size), __FILE__, __LINE__, __func__)
#define XFREE(ptr) do { free(ptr); ptr = NULL; } while(0)

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
#ifdef WITH_SOAPYSDR
	INPUT_SOAPYSDR,
#endif
	INPUT_FILE,
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
int bitstream_append_msbfirst(bitstream_t *bs, const uint8_t *v, const uint32_t numbytes, const uint32_t numbits);
int bitstream_append_lsbfirst(bitstream_t *bs, const uint8_t *bytes, const uint32_t numbytes, const uint32_t numbits);
int bitstream_read_lsbfirst(bitstream_t *bs, uint8_t *bytes, const uint32_t numbytes, const uint32_t numbits);
int bitstream_read_word_msbfirst(bitstream_t *bs, uint32_t *ret, const uint32_t numbits);
int bitstream_copy_next_frame(bitstream_t *src, bitstream_t *dst);
void bitstream_descramble(bitstream_t *bs, uint16_t *lfsr);
void bitstream_reset(bitstream_t *bs);
void bitstream_destroy(bitstream_t *bs);
uint32_t reverse(uint32_t v, int numbits);

// decode.c
void decode_vdl_frame(vdl2_channel_t *v);

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

// output.c
extern FILE *outf;
extern uint8_t hourly, daily, utc, output_raw_frames, dump_asn1, extended_header;
extern int pp_sockfd;
int init_output_file(char *file);
int init_pp(char *pp_addr);
int rotate_outfile();
void output_raw(uint8_t *buf, uint32_t len);

// statsd.c
#ifdef WITH_STATSD
int statsd_initialize(char *statsd_addr);
void statsd_initialize_counters(uint32_t freq);
void statsd_counter_increment(uint32_t freq, char *counter);
void statsd_timing_delta_send(uint32_t freq, char *timer, struct timeval *ts);
#define statsd_increment(freq, counter) do { statsd_counter_increment(freq, counter); } while(0)
#define statsd_timing_delta(freq, timer, start) do { statsd_timing_delta_send(freq, timer, start); } while(0)
#else
#define statsd_increment(freq, counter) nop()
#define statsd_timing_delta(freq, timer, start) nop()
#endif

// util.c
void *xcalloc(size_t nmemb, size_t size, const char *file, const int line, const char *func);
void *xrealloc(void *ptr, size_t size, const char *file, const int line, const char *func);
char *fmt_hexstring(uint8_t *data, uint16_t len);
char *fmt_hexstring_with_ascii(uint8_t *data, uint16_t len);
char *fmt_bitfield(uint8_t val, const dict *d);
size_t slurp_hexstring(char* string, uint8_t **buf);

// dumpvdl2.c
extern uint32_t msg_filter;
extern int do_exit;
extern pthread_barrier_t demods_ready, samples_ready;

// version.c
extern char const * const DUMPVDL2_VERSION;
#endif // !_DUMPVDL2_H
