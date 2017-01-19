#include <stdint.h>
#include "tlv.h"
#define BISPDU_HDR_LEN 30U
#define BISPDU_OPEN_VERSION	1

// BISPDU types
#define BISPDU_TYPE_OPEN	1
#define BISPDU_TYPE_UPDATE	2
#define BISPDU_TYPE_ERROR	3
#define BISPDU_TYPE_KEEPALIVE	4
#define BISPDU_TYPE_CEASE	5
#define BISPDU_TYPE_RIBREFRESH	6

// Error codes
#define BISPDU_ERR_OPEN_PDU 1
#define BISPDU_ERR_UPDATE_PDU 2
#define BISPDU_ERR_TIMER_EXPIRED 3
#define BISPDU_ERR_FSM 4
#define BISPDU_ERR_RIB_REFRESH_PDU 5

typedef struct {
	uint8_t pid;
	uint8_t len[2];		// not using uint16_t to avoid padding and to match PDU octet layout
	uint8_t type;
	uint32_t seq, ack;
	uint8_t coff, cavail;
	uint8_t validation[16];
} idrp_hdr_t;

typedef struct {
	idrp_hdr_t *hdr;
	tlv_list_t *withdrawn_routes, *path_attributes;
	uint8_t *open_src_rdi;
	uint8_t *data;
	uint32_t datalen;
	uint16_t open_holdtime;
	uint16_t open_max_pdu_size;
	uint8_t err_code, err_subcode;
	uint8_t err_fsm_bispdu_type, err_fsm_state;
	uint8_t open_src_rdi_len;
} idrp_pdu_t;

typedef struct {
	char *descr;
	dict *subcodes;
} bispdu_err_t;

// idrp.c
idrp_pdu_t *parse_idrp_pdu(uint8_t *buf, uint32_t len);
void output_idrp(idrp_pdu_t *pdu);
