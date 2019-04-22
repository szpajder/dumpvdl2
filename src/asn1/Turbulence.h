/*
 * Generated by asn1c-0.9.28 (http://lionet.info/asn1c)
 * From ASN.1 module "PMCPDLCMessageSetVersion1"
 * 	found in "atn-cpdlc.asn1"
 * 	`asn1c -fcompound-names -fincludes-quoted -gen-PER`
 */

#ifndef	_Turbulence_H_
#define	_Turbulence_H_


#include "asn_application.h"

/* Including external dependencies */
#include "NativeEnumerated.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dependencies */
typedef enum Turbulence {
	Turbulence_light	= 0,
	Turbulence_moderate	= 1,
	Turbulence_severe	= 2
} e_Turbulence;

/* Turbulence */
typedef long	 Turbulence_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_Turbulence;
asn_struct_free_f Turbulence_free;
asn_struct_print_f Turbulence_print;
asn_constr_check_f Turbulence_constraint;
ber_type_decoder_f Turbulence_decode_ber;
der_type_encoder_f Turbulence_encode_der;
xer_type_decoder_f Turbulence_decode_xer;
xer_type_encoder_f Turbulence_encode_xer;
per_type_decoder_f Turbulence_decode_uper;
per_type_encoder_f Turbulence_encode_uper;

#ifdef __cplusplus
}
#endif

#endif	/* _Turbulence_H_ */
#include "asn_internal.h"