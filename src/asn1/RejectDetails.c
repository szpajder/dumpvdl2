/*
 * Generated by asn1c-0.9.28 (http://lionet.info/asn1c)
 * From ASN.1 module "ADSMessageSetVersion2"
 * 	found in "../../../dumpvdl2.asn1/atn-b2_adsc_v2.asn1"
 * 	`asn1c -fcompound-names -fincludes-quoted -gen-PER`
 */

#include "RejectDetails.h"

static asn_per_constraints_t asn_PER_type_RejectDetails_constr_1 GCC_NOTUSED = {
	{ APC_CONSTRAINED | APC_EXTENSIBLE,  3,  3,  0,  7 }	/* (0..7,...) */,
	{ APC_UNCONSTRAINED,	-1, -1,  0,  0 },
	0, 0	/* No PER value map */
};
static asn_TYPE_member_t asn_MBR_RejectDetails_1[] = {
	{ ATF_NOFLAGS, 0, offsetof(struct RejectDetails, choice.aDS_service_unavailable),
		(ASN_TAG_CLASS_CONTEXT | (0 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_NULL,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"aDS-service-unavailable"
		},
	{ ATF_NOFLAGS, 0, offsetof(struct RejectDetails, choice.undefined_reason),
		(ASN_TAG_CLASS_CONTEXT | (1 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_NULL,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"undefined-reason"
		},
	{ ATF_NOFLAGS, 0, offsetof(struct RejectDetails, choice.maximum_capacity_exceeded),
		(ASN_TAG_CLASS_CONTEXT | (2 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_NULL,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"maximum-capacity-exceeded"
		},
	{ ATF_NOFLAGS, 0, offsetof(struct RejectDetails, choice.reserved),
		(ASN_TAG_CLASS_CONTEXT | (3 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_NULL,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"reserved"
		},
	{ ATF_NOFLAGS, 0, offsetof(struct RejectDetails, choice.waypoint_in_request_not_on_the_route),
		(ASN_TAG_CLASS_CONTEXT | (4 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_NULL,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"waypoint-in-request-not-on-the-route"
		},
	{ ATF_NOFLAGS, 0, offsetof(struct RejectDetails, choice.aDS_contract_not_supported),
		(ASN_TAG_CLASS_CONTEXT | (5 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_NULL,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"aDS-contract-not-supported"
		},
	{ ATF_NOFLAGS, 0, offsetof(struct RejectDetails, choice.noneOfReportTypesSupported),
		(ASN_TAG_CLASS_CONTEXT | (6 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_NULL,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"noneOfReportTypesSupported"
		},
	{ ATF_NOFLAGS, 0, offsetof(struct RejectDetails, choice.noneOfEventTypesSupported),
		(ASN_TAG_CLASS_CONTEXT | (7 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_NULL,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"noneOfEventTypesSupported"
		},
};
static const asn_TYPE_tag2member_t asn_MAP_RejectDetails_tag2el_1[] = {
    { (ASN_TAG_CLASS_CONTEXT | (0 << 2)), 0, 0, 0 }, /* aDS-service-unavailable */
    { (ASN_TAG_CLASS_CONTEXT | (1 << 2)), 1, 0, 0 }, /* undefined-reason */
    { (ASN_TAG_CLASS_CONTEXT | (2 << 2)), 2, 0, 0 }, /* maximum-capacity-exceeded */
    { (ASN_TAG_CLASS_CONTEXT | (3 << 2)), 3, 0, 0 }, /* reserved */
    { (ASN_TAG_CLASS_CONTEXT | (4 << 2)), 4, 0, 0 }, /* waypoint-in-request-not-on-the-route */
    { (ASN_TAG_CLASS_CONTEXT | (5 << 2)), 5, 0, 0 }, /* aDS-contract-not-supported */
    { (ASN_TAG_CLASS_CONTEXT | (6 << 2)), 6, 0, 0 }, /* noneOfReportTypesSupported */
    { (ASN_TAG_CLASS_CONTEXT | (7 << 2)), 7, 0, 0 } /* noneOfEventTypesSupported */
};
static asn_CHOICE_specifics_t asn_SPC_RejectDetails_specs_1 = {
	sizeof(struct RejectDetails),
	offsetof(struct RejectDetails, _asn_ctx),
	offsetof(struct RejectDetails, present),
	sizeof(((struct RejectDetails *)0)->present),
	asn_MAP_RejectDetails_tag2el_1,
	8,	/* Count of tags in the map */
	0,
	8	/* Extensions start */
};
asn_TYPE_descriptor_t asn_DEF_RejectDetails = {
	"RejectDetails",
	"RejectDetails",
	CHOICE_free,
	CHOICE_print,
	CHOICE_constraint,
	CHOICE_decode_ber,
	CHOICE_encode_der,
	CHOICE_decode_xer,
	CHOICE_encode_xer,
	CHOICE_decode_uper,
	CHOICE_encode_uper,
	CHOICE_outmost_tag,
	0,	/* No effective tags (pointer) */
	0,	/* No effective tags (count) */
	0,	/* No tags (pointer) */
	0,	/* No tags (count) */
	&asn_PER_type_RejectDetails_constr_1,
	asn_MBR_RejectDetails_1,
	8,	/* Elements count */
	&asn_SPC_RejectDetails_specs_1	/* Additional specs */
};
