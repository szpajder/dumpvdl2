/*
 * Generated by asn1c-0.9.28 (http://lionet.info/asn1c)
 * From ASN.1 module "ADSMessageSetVersion2"
 * 	found in "../../../dumpvdl2.asn1/atn-b2_adsc_v2.asn1"
 * 	`asn1c -fcompound-names -fincludes-quoted -gen-PER`
 */

#ifndef	_AirspeedChangeTolerance_H_
#define	_AirspeedChangeTolerance_H_


#include "asn_application.h"

/* Including external dependencies */
#include "IasTolerance.h"
#include "MachNumberTolerance.h"
#include "constr_SEQUENCE.h"

#ifdef __cplusplus
extern "C" {
#endif

/* AirspeedChangeTolerance */
typedef struct AirspeedChangeTolerance {
	IasTolerance_t	*ias	/* OPTIONAL */;
	MachNumberTolerance_t	*mach_number	/* OPTIONAL */;
	
	/* Context for parsing across buffer boundaries */
	asn_struct_ctx_t _asn_ctx;
} AirspeedChangeTolerance_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_AirspeedChangeTolerance;

#ifdef __cplusplus
}
#endif

#endif	/* _AirspeedChangeTolerance_H_ */
#include "asn_internal.h"
