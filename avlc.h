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
	"UI",     "(0x01)", "(0x02)", "DM",     "(0x04)", "(0x05)", "(0x06)", "(0x07)",
	"(0x08)", "(0x09)", "(0x0a)", "(0x0b)", "(0x0c)", "(0x0d)", "(0x0e)", "(0x0f)",
	"DISC",   "(0x11)", "(0x12)", "(0x13)", "(0x14)", "(0x15)", "(0x16)", "(0x17)",
	"(0x18)", "(0x19)", "(0x1a)", "(0x1b)", "(0x1c)", "(0x1d)", "(0x1e)", "(0x1f)",
	"(0x20)", "FRMR",   "(0x22)", "(0x23)", "(0x24)", "(0x25)", "(0x26)", "(0x27)",
	"(0x28)", "(0x29)", "(0x2a)", "XID",    "(0x2c)", "(0x2d)", "(0x2e)", "(0x2f)",
	"(0x30)", "(0x31)", "(0x32)", "(0x33)", "(0x34)", "(0x35)", "(0x36)", "(0x37)",
	"TEST"
};

#define UI		0x00
#define DM		0x03
#define DISC	0x10
#define FRMR	0x21
#define XID		0x2b
#define TEST	0x38

// vim: ts=4
