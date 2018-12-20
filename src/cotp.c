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
#include <glib.h>
#include "dumpvdl2.h"
#include "tlv.h"
#include "cotp.h"
#include "icao.h"

static void cotp_pdu_free(gpointer pdu) {
	if(pdu == NULL) return;
/*	cotp_pdu_t *p = (cotp_pdu_t *)pdu;
	if(p->data_valid)
		XFREE(p->data); */
	XFREE(pdu);
}

static int parse_cotp_pdu(cotp_pdu_t *pdu, uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	int final_pdu = 0;
	uint8_t li = buf[0];
	buf++; len--;
	if(li == 0 || li == 255) {
		debug_print("invalid header length indicator: %u\n", li);
		return -1;
	}
	if(len < li) {
		debug_print("header truncated: len %u < li %u\n", len, li);
		return -1;
	}
	uint8_t code = buf[0];
	switch(code & 0xf0) {
	case COTP_TPDU_CR:
	case COTP_TPDU_CC:
	case COTP_TPDU_AK:
	case COTP_TPDU_RJ:
		pdu->code = code & 0xf0;
		break;
	case COTP_TPDU_DT:
		pdu->code = code & 0xfe;
		break;
	default:
		pdu->code = code;
	}
	debug_print("TPDU code: 0x%02x\n", pdu->code);
	switch(pdu->code) {
	case COTP_TPDU_CR:
	case COTP_TPDU_CC:
	case COTP_TPDU_DR:
		if(len < 6) {
			debug_print("Truncated TPDU: code: 0x%02x len: %u\n", pdu->code, len);
			return -1;
		}
		if(pdu->code == COTP_TPDU_DR)
			pdu->class_or_status = buf[5];		// reason
		else
			pdu->class_or_status = buf[5] >> 4;	// protocol class
		final_pdu = 1;
		break;
	case COTP_TPDU_ER:
		if(len < 4) {
			debug_print("Truncated TPDU: code: 0x%02x len: %u\n", pdu->code, len);
			return -1;
		}
		pdu->class_or_status = buf[3];			// reject cause
		break;
	case COTP_TPDU_DT:
	case COTP_TPDU_ED:
		final_pdu = 1;
		break;
	case COTP_TPDU_DC:
	case COTP_TPDU_AK:
	case COTP_TPDU_EA:
	case COTP_TPDU_RJ:
		break;
	default:
		debug_print("Unknown TPDU code 0x%02x\n", pdu->code);
		return -1;
	}
	pdu->cotp_tpdu_valid = 1;
	if(final_pdu) {
// user data is allowed in this PDU; if it's there, try to parse it
		buf += li; len -= li;
		if(len > 0) {
			pdu->data = parse_icao_apdu(buf, len, msg_type);
			if(pdu->data != NULL) {
				pdu->data_valid = 1;
			} else {
				pdu->data_valid = 0;
				pdu->data = buf;
				pdu->datalen = len;
			}
		}
		return 1 + li + len;	// whole buffer consumed
	}
// consume TPDU header only; next TPDU may be present
	return 1 + li;
}

GSList *parse_cotp_concatenated_pdu(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	static GSList *pdu_list = NULL;
	cotp_pdu_t *pdu = NULL;
	int ret;

	if(pdu_list != NULL) {
		g_slist_free_full(pdu_list, cotp_pdu_free);
		pdu_list = NULL;
	}
	while(len > 0) {
		debug_print("Remaining length: %u\n", len);
		pdu = XCALLOC(1, sizeof(cotp_pdu_t));
		pdu_list = g_slist_append(pdu_list, pdu);
		if((ret = parse_cotp_pdu(pdu, buf, len, msg_type)) < 0) {
// parsing failed; store raw buffer in the PDU struct for raw output
			pdu->data = buf;
			pdu->datalen = len;
			break;
		}
		buf += ret; len -= ret;
	}
	return pdu_list;
}

static const dict cotp_tpdu_codes[] = {
	{ COTP_TPDU_CR, "Connect Request" },
	{ COTP_TPDU_CC, "Connect Confirm" },
	{ COTP_TPDU_DR, "Disconnect Request" },
	{ COTP_TPDU_DC, "Disconnect Confirm" },
	{ COTP_TPDU_DT, "Data" },
	{ COTP_TPDU_ED, "Expedited Data" },
	{ COTP_TPDU_AK, "Data Acknowledgement" },
	{ COTP_TPDU_EA, "Expedited Data Acknowledgement" },
	{ COTP_TPDU_RJ, "Reject" },
	{ COTP_TPDU_ER, "Error" },
	{ 0,  NULL }
};

static const dict cotp_dr_reasons[] = {
	{   0,	"Reason not specified" },
	{   1,	"TSAP congestion" },
	{   2,	"Session entity not attached to TSAP" },
	{   3,	"Unknown address" },
	{ 128,	"Normal disconnect" },
	{ 129,  "Remote transport entity congestion" },
	{ 130,	"Connection negotiation failed" },
	{ 131,	"Duplicate source reference" },
	{ 132,	"Mismatched references" },
	{ 133,	"Protocol error" },
	{ 135,	"Reference overflow" },
	{ 136,	"Connection request refused" },
	{ 138,	"Header or parameter length invalid" },
	{   0,  NULL }
};

static const dict cotp_er_reject_causes[] = {
	{ 0,	"Reason not specified" },
	{ 1,	"Invalid parameter code" },
	{ 2,	"Invalid TPDU type" },
	{ 3,	"Invalid parameter value" },
	{ 0,  NULL }
};

static void output_cotp_pdu(gpointer p, gpointer user_data) {
// -Wunused-parameter
	(void)user_data;
	cotp_pdu_t *pdu = (cotp_pdu_t *)p;
	char *str;
	if(!pdu->cotp_tpdu_valid) {
		fprintf(outf, "-- Unparseable COTP TPDU\n");
		output_raw(pdu->data, pdu->datalen);
		return;
	}
	char *tpdu_name = (char *)dict_search(cotp_tpdu_codes, pdu->code);
	fprintf(outf, "COTP %s:\n", tpdu_name ? tpdu_name : "(unknown TPDU code 0x%02x)");
	switch(pdu->code) {
	case COTP_TPDU_CR:
	case COTP_TPDU_CC:
		fprintf(outf, " Protocol class: %u\n", pdu->class_or_status);
		break;
	case COTP_TPDU_ER:
		str = (char *)dict_search(cotp_er_reject_causes, pdu->class_or_status);
		fprintf(outf, " Reject cause: %u (%s)\n", pdu->class_or_status, (str ? str : "<unknown>"));
		break;
	case COTP_TPDU_DR:
		str = (char *)dict_search(cotp_dr_reasons, pdu->class_or_status);
		fprintf(outf, " Reason: %u (%s)\n", pdu->class_or_status, (str ? str : "<unknown>"));
		break;
	}
	if(pdu->data != NULL) {
		if(pdu->data_valid)
			output_icao_apdu(pdu->data);
		else
			output_raw(pdu->data, pdu->datalen);
	}
}

void output_cotp_concatenated_pdu(GSList *pdu_list) {
	if(pdu_list == NULL) {
		fprintf(outf, "-- NULL COTP TPDU\n");
		return;
	}
	g_slist_foreach(pdu_list, output_cotp_pdu, NULL);
}
