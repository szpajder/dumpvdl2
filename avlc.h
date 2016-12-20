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

#define UI		0x00
#define DM		0x03
#define DISC	0x10
#define FRMR	0x21
#define XID		0x2b
#define TEST	0x38

enum avlc_protocols { PROTO_ISO_8208, PROTO_ACARS, PROTO_UNKNOWN };
typedef struct {
	avlc_addr_t src;
	avlc_addr_t dst;
	lcf_t lcf;
	enum avlc_protocols proto;
	uint32_t datalen;
	void  *data;
} avlc_frame_t;

// output.c
void output_avlc(const avlc_frame_t *f);
// vim: ts=4
