/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
 *  Copyright (c) 2017-2019 Tomasz Lemiech <szpajder@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "tlv.h"
#define XID_FMT_ID		0x82
#define XID_GID_PUBLIC		0x80
#define XID_GID_PRIVATE		0xF0
#define XID_MIN_GROUPLEN	3	// group_id + group_len (0)
#define XID_MIN_LEN (1 + 2 * XID_MIN_GROUPLEN)	// XID fmt + empty pub group + empty priv group
#define XID_PARAM_CONN_MGMT	1

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
	enum xid_types type;
	tlv_list_t *pub_params;
	tlv_list_t *vdl_params;
} xid_msg_t;

// xid.c
xid_msg_t *parse_xid(uint8_t cr, uint8_t pf, uint8_t *buf, uint32_t len, uint32_t *msg_type);
void output_xid(xid_msg_t *msg);
