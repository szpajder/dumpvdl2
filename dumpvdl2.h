#ifndef _DUMPVDL2_H
#define _DUMPVDL2_H 1
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include "avlc.h"
#include "tlv.h"

#define DUMPVDL2_VERSION "0.1.0rc"
#define RS_K 249        // Reed-Solomon vector length (bytes)
#define RS_N 255        // Reed-Solomon codeword length (bytes)
#define BSLEN 32768UL
#define TRLEN 17
#define CRCLEN 5
#define HEADER_LEN (3 + TRLEN + CRCLEN)
#define BPS 3
#define LFSR_IV 0x6959u
#define ONES(x) ~(~0 << x)
#define ARITY 8
#define SPS 10
#define SYNC_SYMS 11				// number of symbols searched by correlate_and_sync()
#define PREAMBLE_SYMS 16
#define PREAMBLE_LEN (PREAMBLE_SYMS * BPS)	// preamble length in bits
#define MAX_PREAMBLE_ERRORS 3
#define SYMBOL_RATE 10500
#define CSC_FREQ 136975000U
#define MAX_CHANNELS 8
#define FILE_BUFSIZE 320000U
#define FILE_OVERSAMPLE 10
#define SDR_AUTO_GAIN -100.0f
#define BUFSIZE (1000 * SPS)
#define MAG_LP 0.9f
#define DPHI_LP 0.95f
#define NF_LP 0.85f
#define IQ_LP 0.92f

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
#if WITH_RTLSDR
#define __OPT_RTLSDR			11
#endif
#if WITH_MIRISDR || WITH_RTLSDR
#define __OPT_GAIN			12
#define __OPT_CORRECTION		13
#endif
#if USE_STATSD
#define __OPT_STATSD			14
#endif
#define __OPT_MSG_FILTER		15
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

typedef struct {
	char *token;
	uint32_t value;
	char *description;
} msg_filterspec_t;

#define debug_print(fmt, ...) \
	do { if (DEBUG) fprintf(stderr, "%s(): " fmt, __func__, __VA_ARGS__); } while (0)

#define debug_print_buf_hex(buf, len, fmt, ...) \
	do { \
		if (DEBUG) { \
			fprintf(stderr, "%s(): " fmt, __func__, __VA_ARGS__); \
			fprintf(stderr, "%s(): ", __func__); \
			for(int zz = 0; zz < (len); zz++) { \
				fprintf(stderr, "%02x ", buf[zz]); \
				if(zz && (zz+1) % 32 == 0) fprintf(stderr, "\n%s(): ", __func__); \
			} \
			fprintf(stderr, "\n"); \
		} \
	} while(0)

#define XCALLOC(nmemb, size) xcalloc((nmemb), (size), __FILE__, __LINE__, __func__)
#define XREALLOC(ptr, size) xrealloc((ptr), (size), __FILE__, __LINE__, __func__)

typedef struct {
	uint8_t *buf;
	uint32_t start, end, len, descrambler_pos;
} bitstream_t;

enum demod_states { DM_INIT, DM_SYNC, DM_IDLE };
enum decoder_states { DEC_PREAMBLE, DEC_HEADER, DEC_DATA, DEC_IDLE };
enum input_types {
#if WITH_RTLSDR
	INPUT_RTLSDR,
#endif
#if WITH_MIRISDR
	INPUT_MIRISDR,
#endif
	INPUT_FILE,
	INPUT_UNDEF
};
enum sample_formats { SFMT_U8, SFMT_S16_LE, SFMT_UNDEF };

typedef struct {
	float mag_buf[BUFSIZE];
	float mag_lpbuf[BUFSIZE];		// temporary for testing
	float lp_re, lp_im;
	float I[BUFSIZE];
	float Q[BUFSIZE];
	float pI, pQ;
	float mag_lp;
	float mag_nf;
	float mag_frame;
	float dphi;
	int bufnum, samplenum;
	int cnt, nfcnt;
	int sq;
	int bufs, bufe;
	int sclk;
	int offset_tuning;
	enum demod_states demod_state;
	enum decoder_states decoder_state;
	uint32_t freq;
	uint32_t dm_phi, dm_dphi;
	uint32_t requested_samples;
	uint32_t requested_bits;
	bitstream_t *bs;
	uint32_t datalen, datalen_octets, last_block_len_octets, fec_octets;
	uint32_t num_blocks;
	uint16_t lfsr;
	uint16_t oversample;
	struct timeval tstart;
} vdl2_channel_t;

typedef struct {
	float *sbuf;
	int num_channels;
	vdl2_channel_t **channels;
} vdl2_state_t;

// bitstream.c
bitstream_t *bitstream_init(uint32_t len);
int bitstream_append_msbfirst(bitstream_t *bs, const uint8_t *v, const uint32_t numbytes, const uint32_t numbits);
int bitstream_append_lsbfirst(bitstream_t *bs, const uint8_t *bytes, const uint32_t numbytes, const uint32_t numbits);
int bitstream_read_lsbfirst(bitstream_t *bs, uint8_t *bytes, const uint32_t numbytes, const uint32_t numbits);
int bitstream_read_word_msbfirst(bitstream_t *bs, uint32_t *ret, const uint32_t numbits);
int bitstream_hdlc_unstuff(bitstream_t *bs);
void bitstream_descramble(bitstream_t *bs, uint16_t *lfsr);
void bitstream_reset(bitstream_t *bs);
uint32_t reverse(uint32_t v, int numbits);

// decode.c
void decode_vdl_frame(vdl2_channel_t *v);

// demod.c
vdl2_channel_t *vdl2_channel_init(uint32_t centerfreq, uint32_t freq, uint32_t source_rate, uint32_t oversample);
void sincosf_lut_init();
void process_buf_uchar_init();
void process_buf_uchar(unsigned char *buf, uint32_t len, void *ctx);
void process_buf_short_init();
void process_buf_short(unsigned char *buf, uint32_t len, void *ctx);

// crc.c
uint16_t crc16_ccitt(uint8_t *data, uint32_t len);

// avlc.c
void parse_avlc_frames(vdl2_channel_t *v, uint8_t *buf, uint32_t len);
uint32_t parse_dlc_addr(uint8_t *buf);
void output_avlc(vdl2_channel_t *v, const avlc_frame_t *f, const uint32_t msg_type);

// rs.c
int rs_init();
int rs_verify(uint8_t *data, int fec_octets);

// output.c
extern FILE *outf;
extern uint8_t hourly, daily;
int init_output_file(char *file);
int rotate_outfile();
void output_raw(uint8_t *buf, uint32_t len);

// statsd.c
#if USE_STATSD
int statsd_initialize(char *statsd_addr);
void statsd_initialize_counters(uint32_t freq);
#endif
void statsd_counter_increment(uint32_t freq, char *counter);
void statsd_timing_delta_send(uint32_t freq, char *timer, struct timeval *ts);
#define statsd_increment(freq, counter) do { if(USE_STATSD) statsd_counter_increment(freq, counter); } while(0)
#define statsd_timing_delta(freq, timer, start) do { if(USE_STATSD) statsd_timing_delta_send(freq, timer, start); } while(0)

// util.c
void *xcalloc(size_t nmemb, size_t size, const char *file, const int line, const char *func);
void *xrealloc(void *ptr, size_t size, const char *file, const int line, const char *func);
char *fmt_hexstring(uint8_t *data, uint16_t len);
char *fmt_hexstring_with_ascii(uint8_t *data, uint16_t len);
char *fmt_bitfield(uint8_t val, const dict *d);

// dumpvdl2.c
extern uint32_t msg_filter;
#endif // !_DUMPVDL2_H
