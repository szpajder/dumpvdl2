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
#define FILE_BUFSIZE 320000U
#define FILE_OVERSAMPLE 10
#define SDR_AUTO_GAIN -100
#define BUFSIZE (1000 * SPS)
#define MAG_LP 0.9f
#define DPHI_LP 0.95f
#define NF_LP 0.85f
#define IQ_LP 0.92f

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
	float I[BUFSIZE];
	float Q[BUFSIZE];
	float pI, pQ;
	float mag_lp;
	float mag_nf;
	float mag_frame;
	float dphi;
	int sq;
	int bufs, bufe;
	int sclk;
	int offset_tuning;
	enum demod_states demod_state;
	enum decoder_states decoder_state;
	uint32_t dm_phi, dm_dphi;
	uint32_t requested_samples;
	uint32_t requested_bits;
	bitstream_t *bs;
	float *sbuf;
	uint32_t slen;
	uint32_t datalen, datalen_octets, last_block_len_octets, fec_octets;
	uint32_t num_blocks;
	uint16_t lfsr;
	uint16_t oversample;
	struct timeval tstart;
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
void decode_vdl_frame(vdl2_state_t *v);

// demod.c
vdl2_state_t *vdl2_init(uint32_t centerfreq, uint32_t freq, uint32_t source_rate, uint32_t oversample);
void sincosf_lut_init();
void process_buf_uchar_init();
void process_buf_uchar(unsigned char *buf, uint32_t len, void *ctx);
void process_buf_short_init();
void process_buf_short(unsigned char *buf, uint32_t len, void *ctx);

// crc.c
uint16_t crc16_ccitt(uint8_t *data, uint32_t len);

// avlc.c
void parse_avlc_frames(vdl2_state_t *v, uint8_t *buf, uint32_t len);
uint32_t parse_dlc_addr(uint8_t *buf);
void output_avlc(vdl2_state_t *v, const avlc_frame_t *f);

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
void statsd_counter_increment(char *counter);
void statsd_timing_delta_send(char *timer, struct timeval *ts);
#define statsd_increment(counter) do { if(USE_STATSD) statsd_counter_increment(counter); } while(0)
#define statsd_timing_delta(timer, start) do { if(USE_STATSD) statsd_timing_delta_send(timer, start); } while(0)

// util.c
void *xcalloc(size_t nmemb, size_t size, const char *file, const int line, const char *func);
void *xrealloc(void *ptr, size_t size, const char *file, const int line, const char *func);
char *fmt_hexstring(uint8_t *data, uint16_t len);
char *fmt_hexstring_with_ascii(uint8_t *data, uint16_t len);
char *fmt_bitfield(uint8_t val, const dict *d);
#endif // !_DUMPVDL2_H
