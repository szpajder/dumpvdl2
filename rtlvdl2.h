#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include "avlc.h"

#define RTLVDL2_VERSION "0.1.0rc"
#define RS_K 249        // Reed-Solomon vector dimension (bytes)
#define RS_N 255        // Reed-Solomon code length (bytes)
#define RW 0
#define BSLEN 32768UL		// FIXME
#define OSLEN 8192UL		// FIXME
#define TRLEN 17
#define CRCLEN 5
#define HEADER_LEN (3+ TRLEN + CRCLEN)
#define BPS 3
#define LFSR_IV 0x6959u
#define ONES(x) ~(~0 << x)
#define ARITY 8
#define SPS 10
#define SYNC_SYMS 11							// number of symbols searched by correlate_and_sync()
#define PREAMBLE_SYMS 16
#define PREAMBLE_LEN (PREAMBLE_SYMS * BPS)		// preamble length in bits
#define RTL_BUFSIZE 320000
#define RTL_BUFCNT 15
#define SYMBOL_RATE 10500
#define RTL_OVERSAMPLE 10
#define RTL_RATE (SYMBOL_RATE * SPS * RTL_OVERSAMPLE)
#define RTL_AUTO_GAIN -100
// FIXME
#define BUFSIZE (1000 * SPS)
#define MAG_LP 0.9f
#define DPHI_LP 0.9f
#define NF_LP 0.97f
#define IQ_LP 0.95f

/* #define debug_print(fmt, ...) \
	do { if (DEBUG) fprintf(debugf, "%s(): " fmt, __func__, __VA_ARGS__); fflush(debugf); } while (0) */
#define debug_print(fmt, ...) \
	do { if (DEBUG) fprintf(stderr, "%s(): " fmt, __func__, __VA_ARGS__); } while (0)
#define debug_print_buf_hex2(prefix, buf, len) \
	do { \
		if (DEBUG) { \
			fprintf(stderr, "%s(): %s\n%s(): ", __func__, prefix, __func__); \
			for(int zz = 0; zz < (len); zz++) { \
				fprintf(stderr, "%02x ", buf[zz]); \
				if(zz && (zz+1) % 32 == 0) fprintf(stderr, "\n%s(): ", __func__); \
			} \
			fprintf(stderr, "\n"); \
		} \
	} while(0)

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
typedef struct {
	float mag_buf[BUFSIZE];
	float mag_lpbuf[BUFSIZE];		// FIXME: skasowac po upewnieniu sie ze dziala poprawnie
	float I[BUFSIZE];
	float Q[BUFSIZE];
	float pI, pQ;
	float mag_lp;
	float mag_nf;
	float dphi;
	int sq;							// potrzebne?
	int bufs, bufe;
	int sclk;
	enum demod_states demod_state;
	enum decoder_states decoder_state;
	uint32_t requested_samples;
	uint32_t requested_bits;
	bitstream_t *bs;
	uint32_t symcnt;				// skasowac po testach
	uint32_t datalen, datalen_octets, last_block_len_octets, fec_octets;
	uint32_t num_blocks;
	uint16_t lfsr;
} vdl2_state_t;

// bitstream.c
// FIXME: wykasowac, czego nie uzywamy
bitstream_t *bitstream_init(uint32_t len);
int bitstream_append_msbfirst(bitstream_t *bs, const uint8_t *v, const uint32_t numbytes, const uint32_t numbits);
int bitstream_append_lsbfirst(bitstream_t *bs, const uint8_t *bytes, const uint32_t numbytes, const uint32_t numbits);
int bitstream_read_msbfirst(bitstream_t *bs, uint8_t *bytes, const uint32_t numbytes, const uint32_t numbits);
// FIXME: rename
int bitstream_read_msbfirst2(bitstream_t *bs, uint8_t *bytes, const uint32_t numreadbits, const uint32_t numbits);
int bitstream_read_msbfirst_pad(bitstream_t *bs, uint8_t *bytes, const uint32_t numbytes, const uint32_t numbits);
int bitstream_read_lsbfirst(bitstream_t *bs, uint8_t *bytes, const uint32_t numbytes, const uint32_t numbits);
int bitstream_read_word_msbfirst(bitstream_t *bs, uint32_t *ret, const uint32_t numbits);
int bitstream_hdlc_unstuff(bitstream_t *bs);
void bitstream_descramble(bitstream_t *bs, uint16_t *lfsr);
void bitstream_reset(bitstream_t *bs);
uint32_t reverse(uint32_t v, int numbits);

// decode.c
void decode_vdl_frame(vdl2_state_t *v);

// crc.c
uint16_t crc16_ccitt(uint8_t *data, uint32_t len);

// deinterleave.c
// FIXME: przeniesc do rs.c albo do decode.c
int deinterleave(uint8_t *in, uint32_t len, uint32_t rows, uint32_t cols, uint8_t out[][cols], uint32_t fillwidth, uint32_t offset);

// avlc.c
void parse_avlc_frames(uint8_t *buf, uint32_t len);
int parse_dlc_addr(uint8_t *buf, avlc_addr_t *a, uint8_t final);

// rs.c
int rs_init();
int rs_verify(uint8_t *data, int fec_octets);
void rs_encode(uint8_t *data, uint8_t *parity);

// output.c
extern FILE *outf;
int init_output_file(char *file);
void output_avlc(const avlc_frame_t *f);

// statsd.c
#if USE_STATSD
int statsd_initialize(char *statsd_addr);
void statsd_initialize_counters(uint32_t freq);
#endif
void statsd_counter_increment(char *counter);
#define statsd_increment(counter) do { if(USE_STATSD) statsd_counter_increment(counter); } while(0)

// util.c
void *xcalloc(size_t nmemb, size_t size, const char *file, const int line, const char *func);
void *xrealloc(void *ptr, size_t size, const char *file, const int line, const char *func);

// vim: ts=4
