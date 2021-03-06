/*
 * Generated by asn1c-0.9.28 (http://lionet.info/asn1c)
 * From ASN.1 module "ULCS"
 * 	found in "../../../dumpvdl2.asn1/atn-b1_ulcs.asn1"
 * 	`asn1c -fcompound-names -fincludes-quoted -gen-PER`
 */

#ifndef	_Presentation_context_identifier_H_
#define	_Presentation_context_identifier_H_


#include "asn_application.h"

/* Including external dependencies */
#include "NativeInteger.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dependencies */
typedef enum Presentation_context_identifier {
	Presentation_context_identifier_acse_apdu	= 1,
	Presentation_context_identifier_reserved	= 2,
	Presentation_context_identifier_user_ase_apdu	= 3
} e_Presentation_context_identifier;

/* Presentation-context-identifier */
typedef long	 Presentation_context_identifier_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_Presentation_context_identifier;
asn_struct_free_f Presentation_context_identifier_free;
asn_struct_print_f Presentation_context_identifier_print;
asn_constr_check_f Presentation_context_identifier_constraint;
ber_type_decoder_f Presentation_context_identifier_decode_ber;
der_type_encoder_f Presentation_context_identifier_encode_der;
xer_type_decoder_f Presentation_context_identifier_decode_xer;
xer_type_encoder_f Presentation_context_identifier_encode_xer;
per_type_decoder_f Presentation_context_identifier_decode_uper;
per_type_encoder_f Presentation_context_identifier_encode_uper;

#ifdef __cplusplus
}
#endif

#endif	/* _Presentation_context_identifier_H_ */
#include "asn_internal.h"
