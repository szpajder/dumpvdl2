#include <endian.h>
#include <stdint.h>
#define MIN_AVLC_LEN 11
#define AVLC_FLAG 0x7e

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define BSHIFT 24
#elif __BYTE_ORDER == __BIG_ENDIAN
#define BSHIFT 0
#else
#error Unsupported endianness
#endif

typedef union {
	uint32_t val;
	struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
		uint32_t addr:24;
		uint8_t type:3;
		uint8_t status:1;
#elif __BYTE_ORDER == __BIG_ENDIAN
		uint8_t status:1;
		uint8_t type:3;
		uint32_t addr:24;
#endif
	} a_addr;
} avlc_addr_t;

const char *status_ag_descr[] = {
	"Airborne",
	"On ground"
};

const char *status_cr_descr[] = {
	"Command frame",
	"Response frame"
};

const char *addrtype_descr[] = {
	"reserved",
	"Aircraft",
	"reserved",
	"reserved",
	"Ground station",
	"Ground station",
	"reserved",
	"All stations"
};

// X.25 control field
typedef union {
	uint8_t val;
	struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
		uint8_t type:1;
		uint8_t send_seq:3;
		uint8_t poll:1;
		uint8_t recv_seq:3;
#elif __BYTE_ORDER == __BIG_ENDIAN
		uint8_t recv_seq:3;
		uint8_t poll:1;
		uint8_t send_seq:3;
		uint8_t type:1;
#endif
	} I;
	struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
		uint8_t type:2;
		uint8_t sfunc:2;
		uint8_t pf:1;
		uint8_t recv_seq:3;
#elif __BYTE_ORDER == __BIG_ENDIAN
		uint8_t recv_seq:3;
		uint8_t pf:1;
		uint8_t sfunc:2;
		uint8_t type:2;
#endif
	} S;
	struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
		uint8_t type:2;
		uint8_t mfunc:6;
#elif __BYTE_ORDER == __BIG_ENDIAN
		uint8_t mfunc:6;
		uint8_t type:2;
#endif
	} U;
} lcf_t;

#define IS_I(lcf) ((lcf).val & 0x1) == 0x0
#define IS_S(lcf) ((lcf).val & 0x3) == 0x1
#define IS_U(lcf) ((lcf).val & 0x3) == 0x3
#define U_MFUNC(lcf) (lcf).U.mfunc & 0x3b
#define U_PF(lcf) ((lcf).U.mfunc >> 2) & 0x1

const char *S_cmd[] = {
	"Receive Ready",
	"Receive not Ready",
	"Reject",
	"Selective Reject"
};

const char *U_cmd[] = {
	"UI"
	"unknown (0x01)",
	"unknown (0x02)",
	"DM",
	"unknown (0x04)",
	"unknown (0x05)",
	"unknown (0x06)",
	"unknown (0x07)",
	"DISC",
	"unknown (0x09)",
	"unknown (0x0a)",
	"unknown (0x0b)",
	"unknown (0x0c)",
	"unknown (0x0d)",
	"unknown (0x0e)",
	"unknown (0x0f)",
	"unknown (0x10)",
	"FRMR",
	"unknown (0x12)",
	"unknown (0x13)",
	"unknown (0x14)",
	"unknown (0x15)",
	"unknown (0x16)",
	"XID",
	"unknown (0x18)",
	"unknown (0x19)",
	"unknown (0x1a)",
	"unknown (0x1b)",
	"TEST",
	"unknown (0x1d)",
	"unknown (0x1e)",
	"unknown (0x1f)",
};

#define UI		0x00
#define DM		0x03
#define DISC	0x08
#define FRMR	0x11
#define XID		0x17
#define TEST	0x1c

// vim: ts=4
