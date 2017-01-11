#include <endian.h>
#include <stdint.h>
#include "tlv.h"
#define X25_MIN_LEN 3
#define GFI_X25_MOD8 0x1
#define MAX_X25_ADDR_LEN 8	// bytes
#define MAX_X25_EXT_ADDR_LEN 20	// bytes
#define X25_SNDCF_ID 0xc1
#define X25_SNDCF_VERSION 1
#define MIN_X25_SNDCF_LEN 4

#define SN_PROTO_CLNP_INIT_COMPRESSED	0x01
#define	SN_PROTO_CLNP			0x81
#define	SN_PROTO_ESIS			0x82
#define	SN_PROTO_IDRP			0x85

/*
 * X.25 packet identifiers
 * (ITU-T Rec. X.25, Tab. 5-2/X.25)
 * INTERRUPT, INTERRUPT_CONFIRM and RNR are not listed,
 * because they are not supported in VDL2 (ICAO Doc 9776 6.3.4)
 */
#define X25_CALL_REQUEST	0x0b
#define X25_CALL_ACCEPTED	0x0f
#define X25_CLEAR_REQUEST	0x13
#define X25_CLEAR_CONFIRM	0x17
#define X25_DATA		0x00
#define X25_RR			0x01
#define X25_REJ			0x09
#define X25_RESET_REQUEST	0x1b
#define X25_RESET_CONFIRM	0x1f
#define X25_RESTART_REQUEST	0xfb
#define X25_RESTART_CONFIRM	0xff
#define X25_DIAG		0xf1

typedef struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
	uint8_t chan_group:4;
	uint8_t gfi:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
	uint8_t gfi:4;
	uint8_t chan_group:4;
#else
#error Unsupported endianness
#endif
	uint8_t chan_num;
	union {
		uint8_t val;
		struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
			uint8_t pad:1;
			uint8_t sseq:3;
			uint8_t more:1;
			uint8_t rseq:3;
#elif __BYTE_ORDER == __BIG_ENDIAN
			uint8_t rseq:3;
			uint8_t more:1;
			uint8_t sseq:3;
			uint8_t pad:1;
#endif
		} data;
	} type;
} x25_hdr_t;

typedef struct {
	uint8_t addr[MAX_X25_ADDR_LEN];
	uint8_t len;		// nibbles
} x25_addr_t;

typedef struct {
	x25_hdr_t *hdr;
	uint8_t type;
	uint8_t addr_block_present;
	x25_addr_t calling, called;
	tlv_list_t *facilities;
	uint8_t compression;
	uint8_t clr_cause;
	uint8_t diag_code;
	uint8_t more_data;
	uint8_t rseq, sseq;
	uint8_t proto;
	uint8_t data_valid;
	void *data;
	uint32_t datalen;
} x25_pkt_t;

// x25.c
x25_pkt_t *parse_x25(uint8_t *buf, uint32_t len);
void output_x25(x25_pkt_t *pkt);
