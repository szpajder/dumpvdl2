/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
 *  Copyright (c) 2017 Tomasz Lemiech <szpajder@gmail.com>
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "dumpvdl2.h"
#include "tlv.h"
#include "xid.h"

// list indexed with a bitfield consisting of:
// 4. C/R bit value
// 3. P/F bit value
// 2. connection mgmt parameter h bit value
// 1. connection mgmt parameter r bit value
// GSIF, XID_CMD_LPM and XID_RSP_LPM messages do not contain
// connection mgmt parameter - h and r are forced to 1 in this case
// Reference: ICAO 9776, Table 5.12
static const struct xid_descr xid_names[16] = {
	{ "", "" },
	{ "XID_CMD_LCR", "Link Connection Refused" },
	{ "XID_CMD_HO", "Handoff Request / Broadcast Handoff" },
	{ "GSIF", "Ground Station Information Frame" },
	{ "XID_CMD_LE", "Link Establishment" },
	{ "", "" },
	{ "XID_CMD_HO", "Handoff Initiation" },
	{ "XID_CMD_LPM", "Link Parameter Modification" },
	{ "", "" },
	{ "", "" },
	{ "", "" },
	{ "", "" },
	{ "XID_RSP_LE", "Link Establishment Response" },
	{ "XID_RSP_LCR", "Link Connection Refused Response" },
	{ "XID_RSP_HO", "Handoff Response" },
	{ "XID_RSP_LPM", "Link Parameter Modification Response" }
};

static const vdl_modulation_descr_t modulation_names[] = {
	{ 0x2, "VDL-M2, D8PSK, 31500 bps" },
	{ 0x4, "VDL-M3, D8PSK, 31500 bps" },
	{ 0x0, NULL }
};

static const dict lcr_causes[] = {
	{ 0x00,	"Bad local parameter" },
	{ 0x01, "Out of link layer resources" },
	{ 0x02, "Out of packet layer resources" },
	{ 0x03, "Terrestrial network not available" },
	{ 0x04, "Terrestrial network congestion" },
	{ 0x05, "Cannot support autotune" },
	{ 0x06, "Station cannot support initiating handoff" },
	{ 0x7f, "Other unspecified local reason" },
	{ 0x80, "Bad global parameter" },
	{ 0x81, "Protocol violation" },
	{ 0x82, "Ground system out of resources" },
	{ 0xff, "Other unspecified system reason" },
	{ 0x00, NULL }
};

static char *fmt_vdl_modulation(uint8_t *data, uint16_t len) {
	if(len < 1) return strdup("<empty>");
	char *buf = XCALLOC(64, sizeof(char));
	vdl_modulation_descr_t *ptr;
	for(ptr = (vdl_modulation_descr_t *)modulation_names; ptr->description != NULL; ptr++) {
		if((data[0] & ptr->bit) == ptr->bit) {
			strcat(buf, ptr->description);
			strcat(buf, "; ");
		}
	}
	int slen = strlen(buf);
	if(slen == 0)
		strcat(buf, "<empty>");
	else
		buf[slen-2] = '\0';	// throw out trailing delimiter
	return buf;
}

static char *fmt_vdl_frequency(uint8_t *data, uint16_t len) {
	if(len < 2) return strdup("<empty>");
	char *buf = XCALLOC(64, sizeof(char));
	uint8_t modulation = data[0] >> 4;
	char *modulation_descr = fmt_vdl_modulation(&modulation, 1);
	uint16_t freq = (((uint16_t)data[0] << 8) | data[1]) & 0x0fff;
	uint32_t freq_khz = (freq + 10000) * 10;
	if(freq_khz % 25)
		freq_khz = freq_khz + 25 - freq_khz % 25;
	sprintf(buf, "%.3f MHz (%s)", (float)freq_khz / 1000.f, modulation_descr);
	XFREE(modulation_descr);
	return buf;
}

static char *fmt_dlc_addrs(uint8_t *data, uint16_t len) {
	if(len % 4 != 0) return strdup("<field truncated>");
	uint8_t *ptr = data;
// raw DLC addr is 4 bytes, turn it into 6 hex digits + space, add 1 for \0 at the end
	uint32_t buflen = len / 4 * 7 + 1;
	char *buf = XCALLOC(buflen, sizeof(char));
	char addrstring[8];	// single DLC addr = 6 hex digits + space + \0
	while(len > 0) {
		avlc_addr_t a;
		a.val = parse_dlc_addr(ptr);
		sprintf(addrstring, "%06X ", a.a_addr.addr);
		strcat(buf, addrstring);
		ptr += 4; len -= 4;
	}
	return buf;
}

static char *fmt_freq_support_list(uint8_t *data, uint16_t len) {
	if(len % 6 != 0) return strdup("<field truncated>");
	uint8_t *ptr = data;
	uint32_t buflen = len / 6 * 64;
	char *buf = XCALLOC(buflen, sizeof(char));
	char tmp[64];
	while(len > 0) {
		char *freq = fmt_vdl_frequency(ptr, 2);
		ptr += 2; len -= 2;
		char *gs_addr = fmt_dlc_addrs(ptr, 4);
		ptr += 4; len -= 4;
		sprintf(tmp, "%s(%s); ", gs_addr, freq);
		strcat(buf, tmp);
		XFREE(freq);
		XFREE(gs_addr);
	}
	int slen = strlen(buf);
	if(slen == 0)
		strcat(buf, "<empty>");
	else
		buf[slen-2] = '\0';	// throw out trailing delimiter
	return buf;
}

static char *fmt_string(uint8_t *data, uint16_t len) {
	char *buf = XCALLOC(len + 1, sizeof(char));
	memcpy(buf, data, len);
	buf[len] = '\0';
	return buf;
}

static char *fmt_lcr_cause(uint8_t *data, uint16_t len) {
	if(len < 1) return strdup("<field truncated>");
	char *buf = XCALLOC(128, sizeof(char));
	char *cause_descr = (char *)dict_search(lcr_causes, data[0]);
	sprintf(buf, "0x%02x (%s)", data[0], (cause_descr ? cause_descr : "unknown"));
	data++; len--;
	if(len >= 2) {
		char *delaybuf = XCALLOC(32, sizeof(char));
		sprintf(delaybuf, ", delay: %d", ntohs(*((uint16_t *)data)));
		strcat(buf, delaybuf);
		XFREE(delaybuf);
		data+=2; len-=2;
	}
	if(len > 0) {
		char *additional = fmt_hexstring(data, len);
		strcat(buf, ", additional data: ");
		size_t total_len = strlen(additional) + strlen(buf);
		if(total_len > 127)
			buf = XREALLOC(buf, total_len + 1);
		strcat(buf, additional);
		XFREE(additional);
	}
	return buf;
}

static char *fmt_loc(uint8_t *data, uint16_t len) {
	if(len < 3) return strdup("<field truncated>");
	char *buf = XCALLOC(64, sizeof(char));
// shift to the left end and then back to propagate sign bit
	int lat = ((int)data[0] << 24) | (int)(data[1] << 16);
	lat >>= 20;
	int lon = ((int)data[1] << 28) | ((int)(data[2] & 0xff) << 20);
	lon >>= 20;
	float latf = (float)lat / 10.0f; if(latf < 0) latf = -latf;
	float lonf = (float)lon / 10.0f; if(lonf < 0) lonf = -lonf;
	char ns = (lat < 0 ? 'S' : 'N');
	char we = (lon < 0 ? 'W' : 'E');
	sprintf(buf, "%.1f%c %.1f%c", latf, ns, lonf, we);
	return buf;
}

static char *fmt_loc_alt(uint8_t *data, uint16_t len) {
	if(len < 4) return strdup("<field truncated>");
	char *buf = fmt_loc(data, 3);
	char *altbuf = XCALLOC(32, sizeof(char));
	sprintf(altbuf, " %d ft", (int)data[3] * 1000);
	strcat(buf, altbuf);
	XFREE(altbuf);
	return buf;
}

static const tlv_dict xid_pub_params[] = {
	{ 0x1, &fmt_string, "Parameter set ID" },
	{ 0x2, &fmt_hexstring, "Procedure classes" },
	{ 0x3, &fmt_hexstring, "HDLC options" },
	{ 0x5, &fmt_hexstring, "N1-downlink" },
	{ 0x6, &fmt_hexstring, "N1-uplink" },
	{ 0x7, &fmt_hexstring, "k-downlink" },
	{ 0x8, &fmt_hexstring, "k-uplink" },
	{ 0x9, &fmt_hexstring, "Timer T1_downlink" },
	{ 0xA, &fmt_hexstring, "Counter N2" },
	{ 0xB, &fmt_hexstring, "Timer T2" },
	{ 0xFF, NULL, NULL }
};

static const tlv_dict xid_vdl_params[] = {
	{ 0x00, &fmt_string, "Parameter set ID" },
	{ 0x01, &fmt_hexstring, "Connection management" },
	{ 0x02, &fmt_hexstring, "SQP" },
	{ 0x03, &fmt_hexstring, "XID sequencing" },
	{ 0x04, &fmt_hexstring, "AVLC specific options" },
	{ 0x05, &fmt_hexstring, "Expedited SN connection " },
	{ 0x06, &fmt_lcr_cause, "LCR cause" },
	{ 0x81, &fmt_vdl_modulation, "Modulation support" },
	{ 0x82, &fmt_dlc_addrs, "Alternate ground stations" },
	{ 0x83, &fmt_string, "Destination airport" },
	{ 0x84, &fmt_loc_alt, "Aircraft location" },
	{ 0x40, &fmt_vdl_frequency, "Autotune frequency" },
	{ 0x41, &fmt_dlc_addrs, "Replacement ground stations" },
	{ 0x42, &fmt_hexstring, "Timer T4" },
	{ 0x43, &fmt_hexstring, "MAC persistence" },
	{ 0x44, &fmt_hexstring, "Counter M1" },
	{ 0x45, &fmt_hexstring, "Timer TM2" },
	{ 0x46, &fmt_hexstring, "Timer TG5" },
	{ 0x47, &fmt_hexstring, "Timer T3min" },
	{ 0x48, &fmt_hexstring, "Address filter" },
	{ 0x49, &fmt_hexstring, "Broadcast connection" },
	{ 0xC0, &fmt_freq_support_list, "Frequency support" },
	{ 0xC1, &fmt_string, "Airport coverage" },
	{ 0xC3, &fmt_string, "Nearest airport ID" },
	{ 0xC4, &fmt_hexstring_with_ascii, "ATN router NETs" },
	{ 0xC5, &fmt_hexstring, "System mask" },
	{ 0xC6, &fmt_hexstring, "TG3" },
	{ 0xC7, &fmt_hexstring, "TG4" },
	{ 0xC8, &fmt_loc, "Ground station location" },
	{ 0x00, NULL, NULL }
};

xid_msg_t *parse_xid(uint8_t cr, uint8_t pf, uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	static xid_msg_t *msg = NULL;

	if(len < XID_MIN_LEN) {
		debug_print("%s", "XID too short\n");
		return NULL;
	}
	if(buf[0] != XID_FMT_ID) {
		debug_print("%s", "Unknown XID format\n");
		return NULL;
	}
	buf++; len--;
	if(msg == NULL) {
		msg = XCALLOC(1, sizeof(xid_msg_t));
	} else {
		tlv_list_free(msg->pub_params);
		tlv_list_free(msg->vdl_params);
		msg->pub_params = msg->vdl_params = NULL;
	}
	uint8_t *ptr = buf;
	while(len >= XID_MIN_GROUPLEN) {
		uint8_t gid = *ptr;
		ptr++; len--;
		uint16_t grouplen = (ptr[0] << 8) | ptr[1];
		ptr += 2; len -= 2;
		if(grouplen > len) {
			debug_print("XID group %02x truncated: grouplen=%u buflen=%u\n", gid, grouplen, len);
			return NULL;
		}
		switch(gid) {
		case XID_GID_PUBLIC:
			if(msg->pub_params != NULL) {
				debug_print("Duplicate XID group 0x%02x\n", XID_GID_PUBLIC);
				return NULL;
			}
			msg->pub_params = tlv_deserialize(ptr, grouplen, 1);
			break;
		case XID_GID_PRIVATE:
			if(msg->vdl_params != NULL) {
				debug_print("Duplicate XID group 0x%02x\n", XID_GID_PRIVATE);
				return NULL;
			}
			msg->vdl_params = tlv_deserialize(ptr, grouplen, 1);
			break;
		default:
			debug_print("Unknown XID Group ID 0x%x, ignored\n", gid);
		}
		ptr += grouplen; len -= grouplen;
	}
	if(len > 0)
		debug_print("Warning: %u unparsed octets left at end of XID message\n", len);
// pub_params are optional, vdl_params are mandatory
	if(msg->vdl_params == NULL) {
		debug_print("%s", "Incomplete XID message\n");
		return NULL;
	}
// find connection management parameter to figure out the XID type
	uint8_t cm;
	tlv_list_t *tmp = tlv_list_search(msg->vdl_params, XID_PARAM_CONN_MGMT);
	if(tmp != NULL && tmp->len > 0)
		cm = (tmp->val)[0];
	else
		cm = 0xFF;
	msg->type = ((cr & 0x1) << 3) | ((pf & 0x1) << 2) | ((cm & 0x1) << 1) | ((cm & 0x2) >> 1);
	if(msg->type == GSIF)
		*msg_type |= MSGFLT_XID_GSIF;
	else
		*msg_type |= MSGFLT_XID_NO_GSIF;
	return msg;
}

void output_xid(xid_msg_t *msg) {
	fprintf(outf, "XID: %s\n", xid_names[msg->type].description);
// pub_params are optional, vdl_params are mandatory
	if(msg->pub_params) {
		fprintf(outf, "Public params:\n");
		output_tlv(outf, msg->pub_params, xid_pub_params);
	}
	fprintf(outf, "VDL params:\n");
	output_tlv(outf, msg->vdl_params, xid_vdl_params);
}
