#define XID_FMT_ID 0x82
#define XID_GID_PUBLIC 0x80
#define XID_GID_PRIVATE 0xF0
#define XID_MIN_GROUPLEN 3	// group_id + group_len (0)
#define XID_MIN_LEN (1 + 2 * XID_MIN_GROUPLEN)	// XID fmt + empty pub group + empty priv group

struct xid_descr {
	char *name;
	char *description;
};

enum xid_types {
	XID_CMD_LCR = 1,
	XID_CMD_HO_REQ = 2,
	GSIF = 3,
	XID_CMD_LE = 4,
	XID_CMD_HO_INIT = 6,
	XID_CMD_LPM = 7,
	XID_RSP_LE = 12,
	XID_RSP_LCR = 13,
	XID_RSP_HO = 14,
	XID_RSP_LPM = 15
};

typedef struct {
	uint8_t bit;
	char *description;
} vdl_modulation_descr_t;

typedef struct {
	uint8_t pid;
	char *(*stringify)(uint8_t *, uint8_t);
	char *description;
} xid_param_descr_t;

typedef struct {
	enum xid_types type;
	tlv_list_t *pub_params;
	tlv_list_t *vdl_params;
} xid_msg_t;

// xid.c
xid_msg_t *parse_xid(uint8_t cr, uint8_t pf, uint8_t *buf, uint32_t len);
void output_xid(xid_msg_t *msg);
