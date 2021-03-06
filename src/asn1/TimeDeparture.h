/*
 * Generated by asn1c-0.9.28 (http://lionet.info/asn1c)
 * From ASN.1 module "PMCPDLCMessageSetVersion1"
 * 	found in "../../../dumpvdl2.asn1/atn-b1_cpdlc-v1.asn1"
 * 	`asn1c -fcompound-names -fincludes-quoted -gen-PER`
 */

#ifndef	_TimeDeparture_H_
#define	_TimeDeparture_H_


#include "asn_application.h"

/* Including external dependencies */
#include "DepartureMinimumInterval.h"
#include "constr_SEQUENCE.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct Time;
struct ControlledTime;

/* TimeDeparture */
typedef struct TimeDeparture {
	struct Time	*timeDepartureAllocated	/* OPTIONAL */;
	struct ControlledTime	*timeDepartureControlled	/* OPTIONAL */;
	struct Time	*timeDepartureClearanceExpected	/* OPTIONAL */;
	DepartureMinimumInterval_t	*departureMinimumInterval	/* OPTIONAL */;
	
	/* Context for parsing across buffer boundaries */
	asn_struct_ctx_t _asn_ctx;
} TimeDeparture_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_TimeDeparture;

#ifdef __cplusplus
}
#endif

/* Referred external types */
#include "TimeDepAllocated.h"
#include "ControlledTime.h"
#include "TimeDepClearanceExpected.h"

#endif	/* _TimeDeparture_H_ */
#include "asn_internal.h"
