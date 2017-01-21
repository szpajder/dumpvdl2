#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "esis.h"
#include "rtlvdl2.h"
#include "tlv.h"

static const dict esis_pdu_types[] = {
	{ ESIS_PDU_TYPE_ESH,	"ES Hello" },
	{ ESIS_PDU_TYPE_ISH,	"IS Hello" },
	{ 0,			NULL }
};

static const tlv_dict esis_option_names[] = {
	{ 0xc5,	&fmt_hexstring,		"Security" },
	{ 0xcf, &fmt_hexstring,		"Priority" },
/* QoS Maintenance not used in ATN (ICAO 9705 Table 5.8-2) */
	{ 0x81, &fmt_hexstring,		"Mobile Subnetwork Capabilities" }
};

static int parse_octet_string(uint8_t *buf, uint32_t len, uint8_t **dst, uint8_t *dstlen) {
	if(len == 0) {
		debug_print("%s", "empty buffer\n");
		return -1;
	}
	uint8_t buflen = *buf++; len--;
	if(len < buflen) {
		debug_print("buffer truncated: len %u < expected %u\n", len, buflen);
		return -1;
	}
	*dst = buf;
	*dstlen = buflen;
	return 1 + buflen;	// total number of consumed octets
}

esis_pdu_t *parse_esis_pdu(uint8_t *buf, uint32_t len) {
        static esis_pdu_t *pdu;
        if(len < ESIS_HDR_LEN) {
                debug_print("Too short (len %u < min len %u)\n", len, ESIS_HDR_LEN);
                return NULL;
        }
        if(pdu == NULL) {
                pdu = XCALLOC(1, sizeof(esis_pdu_t));
        } else {
		tlv_list_free(pdu->options);
                memset(pdu, 0, sizeof(esis_pdu_t));
        }
        esis_hdr_t *hdr = (esis_hdr_t *)buf;
	if(hdr->version != 1) {
		debug_print("Unsupported PDU version %u\n", hdr->version);
		return NULL;
	}
        pdu->holdtime = ((uint16_t)hdr->holdtime[0] << 8) | ((uint16_t)hdr->holdtime[1]);
        debug_print("pid: %02x len: %u type: %u holdtime: %u\n",
                hdr->pid, hdr->len, hdr->type, pdu->holdtime);
        if(len < hdr->len) {
                debug_print("Too short (len %u < PDU len %u)\n", len, hdr->len);
                return NULL;
        }
        buf += ESIS_HDR_LEN; len -= ESIS_HDR_LEN;
        debug_print("skipping %u hdr octets, len is now %u\n", ESIS_HDR_LEN, len);

	int ret = parse_octet_string(buf, len, &pdu->net_addr, &pdu->net_addr_len);
	if(ret < 0)
		return NULL;
	buf += ret; len -= ret;
	switch(hdr->type) {
	case ESIS_PDU_TYPE_ESH:
	case ESIS_PDU_TYPE_ISH:
		if(len > 0) {
			pdu->options = tlv_deserialize(buf, len, 1);
			if(pdu->options == NULL)
				return NULL;
		}
		break;
	default:
		debug_print("Unknown PDU type 0x%02x\n", hdr->type);
		return NULL;
	}
	pdu->hdr = hdr;
	return pdu;
}

void output_esis(esis_pdu_t *pdu) {
        esis_hdr_t *hdr = pdu->hdr;
        char *pdu_name = (char *)dict_search(esis_pdu_types, hdr->type);
        fprintf(outf, "ES-IS %s: Hold Time: %u sec\n", pdu_name, pdu->holdtime);

	char *str = fmt_hexstring_with_ascii(pdu->net_addr, pdu->net_addr_len);
	switch(hdr->type) {
	case ESIS_PDU_TYPE_ESH:
		fprintf(outf, " SA : %s\n", str);
		break;
	case ESIS_PDU_TYPE_ISH:
		fprintf(outf, " NET: %s\n", str);
		break;
	}
	free(str);
	if(pdu->options != NULL) {
		fprintf(outf, " Options:\n");
		output_tlv(outf, pdu->options, esis_option_names);
	}
}
