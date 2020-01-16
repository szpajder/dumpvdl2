/*
 * Generated by asn1c-0.9.28 (http://lionet.info/asn1c)
 * From ASN.1 module "ADSMessageSetVersion2"
 * 	found in "../../../dumpvdl2.asn1/atn-b2_adsc_v2.asn1"
 * 	`asn1c -fcompound-names -fincludes-quoted -gen-PER`
 */

#ifndef	_ATSUListHiPrio_H_
#define	_ATSUListHiPrio_H_


#include "asn_application.h"

/* Including external dependencies */
#include "FacilityDesignation.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ATSUListHiPrio */
typedef FacilityDesignation_t	 ATSUListHiPrio_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_ATSUListHiPrio;
asn_struct_free_f ATSUListHiPrio_free;
asn_struct_print_f ATSUListHiPrio_print;
asn_constr_check_f ATSUListHiPrio_constraint;
ber_type_decoder_f ATSUListHiPrio_decode_ber;
der_type_encoder_f ATSUListHiPrio_encode_der;
xer_type_decoder_f ATSUListHiPrio_decode_xer;
xer_type_encoder_f ATSUListHiPrio_encode_xer;
per_type_decoder_f ATSUListHiPrio_decode_uper;
per_type_encoder_f ATSUListHiPrio_encode_uper;

#ifdef __cplusplus
}
#endif

#endif	/* _ATSUListHiPrio_H_ */
#include "asn_internal.h"
