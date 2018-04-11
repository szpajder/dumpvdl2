/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2018 Tomasz Lemiech <szpajder@gmail.com>
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
#include "asn1/BIT_STRING.h"
#include "asn1/ACSE-apdu.h"
#include "asn1/CMAircraftMessage.h"
#include "asn1/CMGroundMessage.h"
#include "asn1/Fully-encoded-data.h"
#include "asn1/AircraftPDUs.h"
#include "asn1/GroundPDUs.h"
#include "asn1/ProtectedAircraftPDUs.h"
#include "asn1/ProtectedGroundPDUs.h"
#include "dumpvdl2.h"			// outf
#include "asn1-util.h"			// asn1_decode_as()
#include "asn1-format-icao.h"		// asn1_output_icao()
#include "icao.h"

#define ACSE_APDU_TYPE_MATCHES(type, value) ((type) == (value) || (type) == ACSE_apdu_PR_NOTHING)
#define APP_TYPE_MATCHES(type, value) ((type) == (value) || (type) == ICAO_APP_TYPE_UNKNOWN)

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
	debug_print("%s", "unable to decode ProtectedAircraftPDU as ATCDownlinkMessage\n");
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
	debug_print("%s", "unable to decode ProtectedGroundPDU as ATCUplinkMessage\n");
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

/* Is this still in use?
 * Disabled, because it clashes with some types of CMAircraftMessages.
		if(APP_TYPE_MATCHES(app_type, ICAO_APP_TYPE_CPC) &&
		   asn1_decode_as(&asn_DEF_AircraftPDUs, (void **)&msg, buf, size) == 0) {
			icao_apdu->type = &asn_DEF_AircraftPDUs;
			icao_apdu->data = msg;
			return;
		}
		ASN_STRUCT_FREE(asn_DEF_AircraftPDUs, msg);
		msg = NULL;
*/
		if(APP_TYPE_MATCHES(app_type, ICAO_APP_TYPE_CMA) &&
		   asn1_decode_as(&asn_DEF_CMAircraftMessage, (void **)&msg, buf, size) == 0) {
			icao_apdu->type = &asn_DEF_CMAircraftMessage;
			icao_apdu->data = msg;
			*msg_type |= MSGFLT_CM;
			return;
		}
		ASN_STRUCT_FREE(asn_DEF_CMAircraftMessage, msg);
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

/* Is this still in use?
 * Disabled, because it clashes with some types of CMAircraftMessages.
		if(APP_TYPE_MATCHES(app_type, ICAO_APP_TYPE_CPC) &&
		   asn1_decode_as(&asn_DEF_GroundPDUs, (void **)&msg, buf, size) == 0) {
			icao_apdu->type = &asn_DEF_GroundPDUs;
			icao_apdu->data = msg;
			return;
		}
		ASN_STRUCT_FREE(asn_DEF_GroundPDUs, msg);
		msg = NULL;
*/
		if(APP_TYPE_MATCHES(app_type, ICAO_APP_TYPE_CMA) &&
		   asn1_decode_as(&asn_DEF_CMGroundMessage, (void **)&msg, buf, size) == 0) {
			icao_apdu->type = &asn_DEF_CMGroundMessage;
			icao_apdu->data = msg;
			*msg_type |= MSGFLT_CM;
			return;
		}
		ASN_STRUCT_FREE(asn_DEF_CMGroundMessage, msg);
		msg = NULL;

	}
	debug_print("%s", "unknown APDU type\n");
}

void decode_ulcs_acse(icao_apdu_t *icao_apdu, uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	ACSE_apdu_t *acse_apdu = NULL;
	asn_dec_rval_t rval;
	rval = uper_decode_complete(0, &asn_DEF_ACSE_apdu, (void **)&acse_apdu, buf, len);
	if(rval.code != RC_OK) {
		fprintf(stderr, "Decoding failed at position %ld\n", (long)rval.consumed);
		goto ulcs_acse_cleanup;
	}
	if(DEBUG)
		asn_fprint(stderr, &asn_DEF_ACSE_apdu, acse_apdu, 1);

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
		debug_print("%s", "No user-information field\n");
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
	if(DEBUG) {
		asn_fprint(stderr, &asn_DEF_Fully_encoded_data, fed, 1);
		debug_print("%ld bytes consumed, %ld left\n", (long)rval.consumed, (long)(len) - (long)rval.consumed);
	}

	if(fed->data.presentation_data_values.present != PDV_list__presentation_data_values_PR_arbitrary) {
		debug_print("%s", "unsupported encoding of fully-encoded-data\n");
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

icao_apdu_t *parse_icao_apdu(uint8_t *buf, uint32_t datalen, uint32_t *msg_type) {
	static icao_apdu_t *icao_apdu = NULL;
	if(datalen < 1) {
		debug_print("APDU too short (len: %d)\n", datalen);
		return NULL;
	}
	if(icao_apdu == NULL) {
		icao_apdu = XCALLOC(1, sizeof(icao_apdu_t));
	} else {
		if(icao_apdu->type != NULL)
			icao_apdu->type->free_struct(icao_apdu->type, icao_apdu->data, 0);
		memset(icao_apdu, 0, sizeof(icao_apdu_t));
	}
	uint8_t *ptr = buf;
	uint32_t len = datalen;
/* Check if it's a X.225 Amdt 1 (1997) Short-form SPDU.
 * All SPDU types have the 8-th bit of SI&P field (the first octet) set to 1. */
	if((ptr[0] & 80) != 0) {
		if(len < 3) {
			debug_print("SPDU too short (len: %d)\n", len);
			goto icao_decoding_failed;
		}
		ptr++; len--;
/* The next octet shall then contain a X.226 Amdt 1 (1997) Presentation layer protocol
 * control information. We only care about two least significant bits, which carry
 * encoding information - 0x2 indicates ASN.1 encoded with Packed Encoding Rules (X.691) */
		if((ptr[0] & 2) != 2) {
			debug_print("Unknown PPDU payload encoding: %u\n", ptr[0] & 2);
			goto icao_decoding_failed;
		}
		ptr++; len--;
/* Decode as ICAO Doc 9705 / X.227 ACSE APDU */
		decode_ulcs_acse(icao_apdu, ptr, len, msg_type);
	} else {
/* Long-Form SPDUs are not used in the ATN, hence this must be a NULL encoding of Session
 * Layer and Presentation Layer, ie. only user data field is present without any header.
 * Decode it as Fully-encoded-data. */
		decode_fully_encoded_data(icao_apdu, ptr, len, msg_type);
	}
	if(icao_apdu->type == NULL) {
icao_decoding_failed:
		icao_apdu->data = buf;
		icao_apdu->datalen = datalen;
	}
// temporary, for debugging
	icao_apdu->raw_data = buf;
	icao_apdu->datalen = datalen;
	return icao_apdu;
}

void output_icao_apdu(icao_apdu_t *icao_apdu) {
	if(icao_apdu == NULL) {
		fprintf(outf, "-- NULL ICAO APDU\n");
		return;
	}
// temporary, for debugging
	output_raw(icao_apdu->raw_data, icao_apdu->datalen);
	if(icao_apdu->type != NULL) {
		if(icao_apdu->data != NULL) {
			if(dump_asn1)
				asn_fprint(outf, icao_apdu->type, icao_apdu->data, 1);
			asn1_output_icao(outf, icao_apdu->type, icao_apdu->data, 0);
		} else {
			fprintf(outf, "%s: <empty PDU>\n", icao_apdu->type->name);
		}
	} else
		output_raw(icao_apdu->data, icao_apdu->datalen);
}
