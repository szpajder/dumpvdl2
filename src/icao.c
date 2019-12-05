/*
 *  This file is a part of dumpvdl2
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <libacars/libacars.h>		// la_proto_node
#include <libacars/vstring.h>		// la_vstring
#include "asn1/BIT_STRING.h"
#include "asn1/ACSE-apdu.h"
#include "asn1/CMAircraftMessage.h"
#include "asn1/CMGroundMessage.h"
#include "asn1/Fully-encoded-data.h"
#include "asn1/ProtectedAircraftPDUs.h"
#include "asn1/ProtectedGroundPDUs.h"
#include "asn1/ATCDownlinkMessage.h"
#include "asn1/ATCUplinkMessage.h"
#include "asn1/ADSAircraftPDUs.h"
#include "asn1/ADSGroundPDUs.h"
#include "asn1/ADSMessage.h"
#include "asn1/ADSReport.h"
#include "asn1/ADSReject.h"
#include "asn1/ADSNonCompliance.h"
#include "asn1/ADSPositiveAcknowledgement.h"
#include "asn1/ADSRequestContract.h"
#include "dumpvdl2.h"
#include "asn1-util.h"			// asn1_decode_as()
#include "asn1-format-icao.h"		// asn1_output_icao_as_text()
#include "icao.h"

#define ACSE_APDU_TYPE_MATCHES(type, value) ((type) == (value) || (type) == ACSE_apdu_PR_NOTHING)
#define APP_TYPE_MATCHES(type, value) ((type) == (value) || (type) == ICAO_APP_TYPE_UNKNOWN)

// Forward declaration
la_type_descriptor const proto_DEF_icao_apdu;

static int decode_ADSAircraftPDU(void **decoded_result, asn_TYPE_descriptor_t **decoded_apdu_type,
uint8_t *buf, int size) {
	ADSAircraftPDUs_t *adsairpdu = NULL;
	int ret = -1;	// error by default

	if(asn1_decode_as(&asn_DEF_ADSAircraftPDUs, (void **)&adsairpdu, buf, size) != 0)
		goto ads_aircraft_pdu_cleanup;

	asn_TYPE_descriptor_t *next_td = NULL;
	ADSMessage_t *next_pdu = NULL;
	switch(adsairpdu->adsAircraftPdu.present) {
	case ADSAircraftPDU_PR_aDS_report_PDU:
	case ADSAircraftPDU_PR_aDS_accepted_PDU:
		next_td = &asn_DEF_ADSReport;
		next_pdu = &adsairpdu->adsAircraftPdu.choice.aDS_report_PDU.ic_report.aDSMessage;
		break;
	case ADSAircraftPDU_PR_aDS_rejected_PDU:
		next_td = &asn_DEF_ADSReject;
		next_pdu = &adsairpdu->adsAircraftPdu.choice.aDS_rejected_PDU.ic_reject.aDSMessage;
		break;
	case ADSAircraftPDU_PR_aDS_ncn_PDU:
		next_td = &asn_DEF_ADSNonCompliance;
		next_pdu = &adsairpdu->adsAircraftPdu.choice.aDS_ncn_PDU.ic_ncn.aDSMessage;
		break;
	case ADSAircraftPDU_PR_aDS_positive_acknowledgement_PDU:
		next_td = &asn_DEF_ADSPositiveAcknowledgement;
		next_pdu = &adsairpdu->adsAircraftPdu.choice.aDS_positive_acknowledgement_PDU.ic_positive_ack.aDSPositiveAck;
		break;
	case ADSAircraftPDU_PR_aDS_cancel_positive_acknowledgement_PDU:
	case ADSAircraftPDU_PR_aDS_cancel_negative_acknowledgement_PDU:
	case ADSAircraftPDU_PR_aDS_provider_abort_PDU:
	case ADSAircraftPDU_PR_aDS_user_abort_PDU:
		*decoded_result = adsairpdu;
		*decoded_apdu_type = &asn_DEF_ADSAircraftPDUs;
		return 0;
	default:
		break;
	}
	if(next_td == NULL || next_pdu == NULL) {
		goto ads_aircraft_pdu_cleanup;
	}
	if(asn1_decode_as(next_td, decoded_result, next_pdu->buf, next_pdu->size) == 0) {
		ret = 0;
		*decoded_apdu_type = next_td;
		goto ads_aircraft_pdu_cleanup;
	}
	debug_print("unable to decode ADSAircraftPDU as %s\n", next_td->name);
ads_aircraft_pdu_cleanup:
	ASN_STRUCT_FREE(asn_DEF_ADSAircraftPDUs, adsairpdu);
	return ret;
}

static int decode_ADSGroundPDU(void **decoded_result, asn_TYPE_descriptor_t **decoded_apdu_type,
uint8_t *buf, int size) {
	ADSGroundPDUs_t *adsgndpdu = NULL;
	int ret = -1;	// error by default

	if(asn1_decode_as(&asn_DEF_ADSGroundPDUs, (void **)&adsgndpdu, buf, size) != 0)
		goto ads_ground_pdu_cleanup;

	asn_TYPE_descriptor_t *next_td = NULL;
	ADSMessage_t *next_pdu = NULL;
	switch(adsgndpdu->adsGroundPdu.present) {
	case ADSGroundPDU_PR_aDS_contract_PDU:
		next_td = &asn_DEF_ADSRequestContract;
		next_pdu = &adsgndpdu->adsGroundPdu.choice.aDS_contract_PDU.ic_contract_request.aDSMessage;
		break;
	case ADSGroundPDU_PR_aDS_cancel_contract_PDU:
	case ADSGroundPDU_PR_aDS_cancel_all_contracts_PDU:
	case ADSGroundPDU_PR_aDS_provider_abort_PDU:
	case ADSGroundPDU_PR_aDS_user_abort_PDU:
		*decoded_result = adsgndpdu;
		*decoded_apdu_type = &asn_DEF_ADSGroundPDUs;
		return 0;
	default:
		break;
	}
	if(next_td == NULL || next_pdu == NULL) {
		goto ads_ground_pdu_cleanup;
	}
	if(asn1_decode_as(next_td, decoded_result, next_pdu->buf, next_pdu->size) == 0) {
		ret = 0;
		*decoded_apdu_type = next_td;
		goto ads_ground_pdu_cleanup;
	}
	debug_print("unable to decode ADSGroundPDU as %s\n", next_td->name);
ads_ground_pdu_cleanup:
	ASN_STRUCT_FREE(asn_DEF_ADSGroundPDUs, adsgndpdu);
	return ret;
}

static int decode_protected_ATCDownlinkMessage(void **decoded_result, asn_TYPE_descriptor_t **decoded_apdu_type,
ACSE_apdu_PR acse_apdu_type, uint8_t *buf, int size) {
	ProtectedAircraftPDUs_t *pairpdu = NULL;
	int ret = -1;	// error by default

	if(asn1_decode_as(&asn_DEF_ProtectedAircraftPDUs, (void **)&pairpdu, buf, size) != 0)
		goto protected_aircraft_pdu_cleanup;

	ProtectedDownlinkMessage_t *p_downlink_msg = NULL;
	switch(pairpdu->present) {
	case ProtectedAircraftPDUs_PR_startdown:
		p_downlink_msg = &pairpdu->choice.startdown.startDownlinkMessage;
		break;
	case ProtectedAircraftPDUs_PR_send:
		p_downlink_msg = &pairpdu->choice.send;
		break;
	case ProtectedAircraftPDUs_PR_abortUser:
	case ProtectedAircraftPDUs_PR_abortProvider:
// First do a check on ACSE APDU type, if present, to avoid possible clashing with
// other message types (eg. CMContactResponse). abortUser and abortProvider shall
// appear in ABRT APDUs only.
		if(!ACSE_APDU_TYPE_MATCHES(acse_apdu_type, ACSE_apdu_PR_abrt))
			break;
// These messages have no ATCDownlinkMessage inside.
// Return the whole ProtectedAircraftPDUs for output.
		*decoded_result = pairpdu;
		*decoded_apdu_type = &asn_DEF_ProtectedAircraftPDUs;
		return 0;
	default:
		break;
	}
	if(p_downlink_msg == NULL)
		goto protected_aircraft_pdu_cleanup;
	if(p_downlink_msg->protectedMessage == NULL) {	// NULL message is valid
		ret = 0;
		*decoded_apdu_type = &asn_DEF_ATCDownlinkMessage;
		goto protected_aircraft_pdu_cleanup;
	}
	if(asn1_decode_as(&asn_DEF_ATCDownlinkMessage, decoded_result,
	   p_downlink_msg->protectedMessage->buf,
	   p_downlink_msg->protectedMessage->size) == 0) {
		ret = 0;
		*decoded_apdu_type = &asn_DEF_ATCDownlinkMessage;
		goto protected_aircraft_pdu_cleanup;
	}
	debug_print("unable to decode ProtectedAircraftPDU as ATCDownlinkMessage\n");
protected_aircraft_pdu_cleanup:
	ASN_STRUCT_FREE(asn_DEF_ProtectedAircraftPDUs, pairpdu);
	return ret;
}

static int decode_protected_ATCUplinkMessage(void **decoded_result, asn_TYPE_descriptor_t **decoded_apdu_type,
ACSE_apdu_PR acse_apdu_type, uint8_t *buf, int size) {
	ProtectedGroundPDUs_t *pgndpdu = NULL;
	int ret = -1;   // error by default

	if(asn1_decode_as(&asn_DEF_ProtectedGroundPDUs, (void **)&pgndpdu, buf, size) != 0)
		goto protected_ground_pdu_cleanup;

	ProtectedUplinkMessage_t *p_uplink_msg = NULL;
	switch(pgndpdu->present) {
	case ProtectedGroundPDUs_PR_startup:
		p_uplink_msg = &pgndpdu->choice.startup;
		break;
	case ProtectedGroundPDUs_PR_send:
		p_uplink_msg = &pgndpdu->choice.send;
		break;
	case ProtectedGroundPDUs_PR_abortUser:
	case ProtectedGroundPDUs_PR_abortProvider:
// First do a check on ACSE APDU type, if present, to avoid possible clashing with
// other message types (eg. CMContactResponse). abortUser and abortProvider shall
// appear in ABRT APDUs only.
		if(!ACSE_APDU_TYPE_MATCHES(acse_apdu_type, ACSE_apdu_PR_abrt))
			break;
// These messages have no ATCUplinkMessage inside.
// Return the whole ProtectedGroundPDUs for output.
		*decoded_result = pgndpdu;
		*decoded_apdu_type = &asn_DEF_ProtectedGroundPDUs;
		return 0;
	default:
		break;
	}
	if(p_uplink_msg == NULL)
		goto protected_ground_pdu_cleanup;
	if(p_uplink_msg->protectedMessage == NULL) {	// NULL message is valid
		ret = 0;
		*decoded_apdu_type = &asn_DEF_ATCUplinkMessage;
		goto protected_ground_pdu_cleanup;
	}
	if(asn1_decode_as(&asn_DEF_ATCUplinkMessage, decoded_result,
	   p_uplink_msg->protectedMessage->buf,
	   p_uplink_msg->protectedMessage->size) == 0) {
		ret = 0;
		*decoded_apdu_type = &asn_DEF_ATCUplinkMessage;
		goto protected_ground_pdu_cleanup;
	}
	debug_print("unable to decode ProtectedGroundPDU as ATCUplinkMessage\n");
protected_ground_pdu_cleanup:
	ASN_STRUCT_FREE(asn_DEF_ProtectedGroundPDUs, pgndpdu);
	return ret;
}

static void decode_arbitrary_payload(icao_apdu_t *icao_apdu, AE_qualifier_form2_t app_type,
ACSE_apdu_PR acse_apdu_type, uint8_t *buf, uint32_t size, uint32_t *msg_type) {
	void *msg = NULL;
	asn_TYPE_descriptor_t *decoded_apdu_type = NULL;
	if(*msg_type & MSGFLT_SRC_AIR) {
		if(APP_TYPE_MATCHES(app_type, ICAO_APP_TYPE_CPC) &&
		   decode_protected_ATCDownlinkMessage((void **)&msg, &decoded_apdu_type, acse_apdu_type, buf, size) == 0) {
			icao_apdu->type = decoded_apdu_type;
			icao_apdu->data = msg;
			*msg_type |= MSGFLT_CPDLC;
			return;
		}
		ASN_STRUCT_FREE(asn_DEF_ATCDownlinkMessage, msg);
		msg = NULL;

		if(APP_TYPE_MATCHES(app_type, ICAO_APP_TYPE_CMA) &&
		   asn1_decode_as(&asn_DEF_CMAircraftMessage, (void **)&msg, buf, size) == 0) {
			icao_apdu->type = &asn_DEF_CMAircraftMessage;
			icao_apdu->data = msg;
			*msg_type |= MSGFLT_CM;
			return;
		}
		ASN_STRUCT_FREE(asn_DEF_CMAircraftMessage, msg);
		msg = NULL;

		if(APP_TYPE_MATCHES(app_type, ICAO_APP_TYPE_ADS) &&
		   decode_ADSAircraftPDU((void **)&msg, &decoded_apdu_type, buf, size) == 0) {
			icao_apdu->type = decoded_apdu_type;
			icao_apdu->data = msg;
			*msg_type |= MSGFLT_ADSC;
			return;
		}
		ASN_STRUCT_FREE(asn_DEF_ADSAircraftPDUs, msg);
		msg = NULL;
	} else {	// MSGFLT_SRC_GND implied
		if(APP_TYPE_MATCHES(app_type, ICAO_APP_TYPE_CPC) &&
		   decode_protected_ATCUplinkMessage((void **)&msg, &decoded_apdu_type, acse_apdu_type, buf, size) == 0) {
			icao_apdu->type = decoded_apdu_type;
			icao_apdu->data = msg;
			*msg_type |= MSGFLT_CPDLC;
			return;
		}
		ASN_STRUCT_FREE(asn_DEF_ATCUplinkMessage, msg);
		msg = NULL;

		if(APP_TYPE_MATCHES(app_type, ICAO_APP_TYPE_CMA) &&
		   asn1_decode_as(&asn_DEF_CMGroundMessage, (void **)&msg, buf, size) == 0) {
			icao_apdu->type = &asn_DEF_CMGroundMessage;
			icao_apdu->data = msg;
			*msg_type |= MSGFLT_CM;
			return;
		}
		ASN_STRUCT_FREE(asn_DEF_CMGroundMessage, msg);
		msg = NULL;

		if(APP_TYPE_MATCHES(app_type, ICAO_APP_TYPE_ADS) &&
		   decode_ADSGroundPDU((void **)&msg, &decoded_apdu_type, buf, size) == 0) {
			icao_apdu->type = decoded_apdu_type;
			icao_apdu->data = msg;
			*msg_type |= MSGFLT_ADSC;
			return;
		}
		ASN_STRUCT_FREE(asn_DEF_ADSGroundPDUs, msg);
		msg = NULL;
	}
	debug_print("unknown APDU type\n");
}

void decode_ulcs_acse(icao_apdu_t *icao_apdu, uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	ACSE_apdu_t *acse_apdu = NULL;
	asn_dec_rval_t rval;
	rval = uper_decode_complete(0, &asn_DEF_ACSE_apdu, (void **)&acse_apdu, buf, len);
	if(rval.code != RC_OK) {
		debug_print("Decoding failed at position %ld\n", (long)rval.consumed);
		goto ulcs_acse_cleanup;
	}
#ifdef DEBUG
	asn_fprint(stderr, &asn_DEF_ACSE_apdu, acse_apdu, 1);
#endif

	AE_qualifier_form2_t ae_qualifier = ICAO_APP_TYPE_UNKNOWN;
	Association_information_t *user_info = NULL;
	switch(acse_apdu->present) {
	case ACSE_apdu_PR_aarq:
		user_info = acse_apdu->choice.aarq.user_information;
		if(acse_apdu->choice.aarq.calling_AE_qualifier != NULL) {
			if(acse_apdu->choice.aarq.calling_AE_qualifier->present == AE_qualifier_PR_ae_qualifier_form2) {
				ae_qualifier = acse_apdu->choice.aarq.calling_AE_qualifier->choice.ae_qualifier_form2;
			}
		}
		break;
	case ACSE_apdu_PR_aare:
		user_info = acse_apdu->choice.aare.user_information;
		break;
	case ACSE_apdu_PR_abrt:
		user_info = acse_apdu->choice.abrt.user_information;
		break;
	case ACSE_apdu_PR_rlre:
		user_info = acse_apdu->choice.rlre.user_information;
		break;
	case ACSE_apdu_PR_rlrq:
		user_info = acse_apdu->choice.rlrq.user_information;
		break;
	default:
		break;
	}
	debug_print("calling-AE-qualifier: %ld\n", ae_qualifier);
	if(user_info == NULL) {
		debug_print("No user-information field\n");
		goto ulcs_acse_cleanup;
	}
	if(user_info->data.encoding.present != EXTERNALt__encoding_PR_arbitrary) {
		debug_print("unsupported encoding: %d\n", user_info->data.encoding.present);
		goto ulcs_acse_cleanup;
	}
	decode_arbitrary_payload(icao_apdu, ae_qualifier, acse_apdu->present,
				 user_info->data.encoding.choice.arbitrary.buf,
				 user_info->data.encoding.choice.arbitrary.size,
				 msg_type);
ulcs_acse_cleanup:
	ASN_STRUCT_FREE(asn_DEF_ACSE_apdu, acse_apdu);
	return;
}

static void decode_fully_encoded_data(icao_apdu_t *icao_apdu, uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	Fully_encoded_data_t *fed = NULL;
	asn_dec_rval_t rval;
	rval = uper_decode_complete(0, &asn_DEF_Fully_encoded_data, (void **)&fed, buf, len);
	if(rval.code != RC_OK) {
		debug_print("uper_decode_complete() failed at position %ld\n", (long)rval.consumed);
		goto fed_cleanup;
	}
#ifdef DEBUG
	asn_fprint(stderr, &asn_DEF_Fully_encoded_data, fed, 1);
#endif
	debug_print("%ld bytes consumed, %ld left\n", (long)rval.consumed, (long)(len) - (long)rval.consumed);

	if(fed->data.presentation_data_values.present != PDV_list__presentation_data_values_PR_arbitrary) {
		debug_print("unsupported encoding of fully-encoded-data\n");
		goto fed_cleanup;
	}
	switch(fed->data.presentation_context_identifier) {
	case Presentation_context_identifier_acse_apdu:
		decode_ulcs_acse(icao_apdu,
				 fed->data.presentation_data_values.choice.arbitrary.buf,
				 fed->data.presentation_data_values.choice.arbitrary.size,
				 msg_type);
		break;
	case Presentation_context_identifier_user_ase_apdu:
// AE-qualifier and ACSE APDU type are unknown here
		decode_arbitrary_payload(icao_apdu, ICAO_APP_TYPE_UNKNOWN, ACSE_apdu_PR_NOTHING,
				 fed->data.presentation_data_values.choice.arbitrary.buf,
				 fed->data.presentation_data_values.choice.arbitrary.size,
				 msg_type);
		break;
	default:
		debug_print("unsupported presentation-context-identifier: %ld\n",
			fed->data.presentation_context_identifier);
		goto fed_cleanup;
	}
fed_cleanup:
	ASN_STRUCT_FREE(asn_DEF_Fully_encoded_data, fed);
	return;
}

la_proto_node *icao_apdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	NEW(icao_apdu_t, icao_apdu);
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_icao_apdu;
	node->data = icao_apdu;
	node->next = NULL;

	icao_apdu->err = true;		// fail-safe default
	if(len < 1) {
		debug_print("APDU too short (len: %u)\n", len);
		goto fail;
	}
	uint8_t *ptr = buf;
	uint32_t remaining = len;
// Check if it's a X.225 Amdt 1 (1997) Short-form SPDU.
// All SPDU types have the 8-th bit of SI&P field (the first octet) set to 1.
	if((ptr[0] & 0x80) != 0) {
		if(remaining < 3) {
			debug_print("Short-form SPDU too short (len: %u < 3)\n", len);
			goto fail;
		}
		icao_apdu->spdu_id = ptr[0] & 0xf8;
		icao_apdu->spdu_special_data = ptr[0] & 0x3;
// The next octet shall then contain a X.226 Amdt 1 (1997) Presentation layer protocol
// control information. We only care about two least significant bits, which carry
// encoding information - 0x2 indicates ASN.1 encoded with Packed Encoding Rules
// Unaligned (X.691)
		if((ptr[1] & 3) != 2) {
			debug_print("Unknown PPDU payload encoding: %u\n", ptr[1] & 3);
			goto fail;
		}
		ptr += 2; remaining -= 2;
// Decode as ICAO Doc 9705 / X.227 ACSE APDU
		decode_ulcs_acse(icao_apdu, ptr, remaining, msg_type);
		if(icao_apdu->type == NULL) {
			goto fail;
		}
	} else {
// Long-Form SPDUs are not used in the ATN, hence this must be a NULL encoding of Session
// Layer and Presentation Layer, ie. only user data field is present without any header.
// Decode it as Fully-encoded-data.
		if(remaining < 1) {
			debug_print("NULL SPDU too short (len: %u < 1)\n", len);
			goto fail;
		}
		decode_fully_encoded_data(icao_apdu, ptr, remaining, msg_type);
		if(icao_apdu->type == NULL) {
			goto fail;
		}
	}
	icao_apdu->err = false;
	return node;
fail:
	node->next = unknown_proto_pdu_new(buf, len);
	return node;
}

#define X225_SPDU_SCN  0xe8
#define X225_SPDU_SAC  0xf0
#define X225_SPDU_SACC 0xd8
#define X225_SPDU_SRF  0xe0
#define X225_SPDU_SRFC 0xa0

static dict const x225_spdu_names[] = {
	{ X225_SPDU_SCN,  "Short Connect" },
	{ X225_SPDU_SAC,  "Short Accept" },
	{ X225_SPDU_SACC, "Short Accept Continue" },
	{ X225_SPDU_SRF,  "Short Refuse" },
	{ X225_SPDU_SRFC, "Short Refuse Continue" },
	{ 0, NULL }
};

void icao_apdu_format_text(la_vstring *vstr, void const * const data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data);
	ASSERT(indent >= 0);

	CAST_PTR(icao_apdu, icao_apdu_t *, data);
	if(icao_apdu->err == true) {
		LA_ISPRINTF(vstr, indent, "%s", "-- Unparseable ICAO APDU\n");
		return;
	}
	if(icao_apdu->spdu_id != 0) {
		char *str = dict_search(x225_spdu_names, icao_apdu->spdu_id);
		if(str != NULL) {
			LA_ISPRINTF(vstr, indent, "X.225 Session SPDU: %s\n", str);
		} else {
			LA_ISPRINTF(vstr, indent, "X.225 Session SPDU: unknown type (0x%02x)\n",
				icao_apdu->spdu_id);
		}
		if(icao_apdu->spdu_id == X225_SPDU_SRF) {
			LA_ISPRINTF(vstr, indent+1, "Refusal: %s\n",
				(icao_apdu->spdu_special_data & 1 ? "persistent" : "transient"));
			LA_ISPRINTF(vstr, indent+1, "Transport connection: %s\n",
				(icao_apdu->spdu_special_data & 2 ? "release" : "retain"));
		}
	}
	if(icao_apdu->data != NULL && icao_apdu->type != NULL) {
		if(dump_asn1) {
			asn_sprintf(vstr, icao_apdu->type, icao_apdu->data, indent);
		}
		asn1_output_icao_as_text(vstr, icao_apdu->type, icao_apdu->data, indent);
	} else {
		LA_ISPRINTF(vstr, indent, "%s: <empty PDU>\n", icao_apdu->type->name);
	}
}

void icao_apdu_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	CAST_PTR(icao_apdu, icao_apdu_t *, data);
	if(icao_apdu->type != NULL) {
		icao_apdu->type->free_struct(icao_apdu->type, icao_apdu->data, 0);
	}
	XFREE(data);
}

la_type_descriptor const proto_DEF_icao_apdu = {
	.format_text = icao_apdu_format_text,
	.destroy  = icao_apdu_destroy
};
