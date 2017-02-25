#include <stdint.h>
#include <endian.h>
#include "tlv.h"

#define ESIS_HDR_LEN		9
#define ESIS_PDU_TYPE_ESH	2
#define ESIS_PDU_TYPE_ISH	4
/* REDIRECT PDU not used in ATN (ICAO 9705 5.8.2.1.4) */

typedef struct {
	uint8_t pid;
	uint8_t len;
	uint8_t version;
	uint8_t reserved;
#if __BYTE_ORDER__ == __LITTLE_ENDIAN
	uint8_t type:5;
	uint8_t pad:3;
#elif __BYTE_ORDER__ == __BIG_ENDIAN
	uint8_t pad:3;
	uint8_t type:5;
#else
#error Unsupported endianness
#endif
	uint8_t holdtime[2];	// not using uint16_t to avoid padding and to match PDU octet layout
	uint8_t cksum[2];
} esis_hdr_t;

typedef struct {
	esis_hdr_t *hdr;
	uint8_t *net_addr;	/* SA for ESH, NET for ISH */
	tlv_list_t *options;
	uint16_t holdtime;
	uint8_t net_addr_len;
} esis_pdu_t;

// esis.c
esis_pdu_t *parse_esis_pdu(uint8_t *buf, uint32_t len, uint32_t *msg_type);
void output_esis(esis_pdu_t *pdu);

