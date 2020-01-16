/*
 * Generated by asn1c-0.9.28 (http://lionet.info/asn1c)
 * From ASN.1 module "PMCPDLCMessageSetVersion1"
 * 	found in "../../../dumpvdl2.asn1/atn-b1_cpdlc-v1.asn1"
 * 	`asn1c -fcompound-names -fincludes-quoted -gen-PER`
 */

#ifndef	_RunwayRVR_H_
#define	_RunwayRVR_H_


#include "asn_application.h"

/* Including external dependencies */
#include "Runway.h"
#include "RVR.h"
#include "constr_SEQUENCE.h"

#ifdef __cplusplus
extern "C" {
#endif

/* RunwayRVR */
typedef struct RunwayRVR {
	Runway_t	 runway;
	RVR_t	 rVR;
	
	/* Context for parsing across buffer boundaries */
	asn_struct_ctx_t _asn_ctx;
} RunwayRVR_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_RunwayRVR;

#ifdef __cplusplus
}
#endif

#endif	/* _RunwayRVR_H_ */
#include "asn_internal.h"
