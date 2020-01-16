/*
 * Generated by asn1c-0.9.28 (http://lionet.info/asn1c)
 * From ASN.1 module "ACSE-1"
 * 	found in "../../../dumpvdl2.asn1/atn-b1_ulcs.asn1"
 * 	`asn1c -fcompound-names -fincludes-quoted -gen-PER`
 */

#ifndef	_ABRT_diagnostic_H_
#define	_ABRT_diagnostic_H_


#include "asn_application.h"

/* Including external dependencies */
#include "NativeEnumerated.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dependencies */
typedef enum ABRT_diagnostic {
	ABRT_diagnostic_no_reason_given	= 1,
	ABRT_diagnostic_protocol_error	= 2,
	ABRT_diagnostic_authentication_mechanism_name_not_recognized	= 3,
	ABRT_diagnostic_authentication_mechanism_name_required	= 4,
	ABRT_diagnostic_authentication_failure	= 5,
	ABRT_diagnostic_authentication_required	= 6
	/*
	 * Enumeration is extensible
	 */
} e_ABRT_diagnostic;

/* ABRT-diagnostic */
typedef long	 ABRT_diagnostic_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_ABRT_diagnostic;
asn_struct_free_f ABRT_diagnostic_free;
asn_struct_print_f ABRT_diagnostic_print;
asn_constr_check_f ABRT_diagnostic_constraint;
ber_type_decoder_f ABRT_diagnostic_decode_ber;
der_type_encoder_f ABRT_diagnostic_encode_der;
xer_type_decoder_f ABRT_diagnostic_decode_xer;
xer_type_encoder_f ABRT_diagnostic_encode_xer;
per_type_decoder_f ABRT_diagnostic_decode_uper;
per_type_encoder_f ABRT_diagnostic_encode_uper;

#ifdef __cplusplus
}
#endif

#endif	/* _ABRT_diagnostic_H_ */
#include "asn_internal.h"
