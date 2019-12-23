/*
 * Generated by asn1c-0.9.28 (http://lionet.info/asn1c)
 * From ASN.1 module "ADSMessageSetVersion2"
 * 	found in "../../../dumpvdl2.asn1/atn-b2_adsc_v2.asn1"
 * 	`asn1c -fcompound-names -fincludes-quoted -gen-PER`
 */

#include "PeriodicContractRequest.h"

static asn_TYPE_member_t asn_MBR_PeriodicContractRequest_1[] = {
	{ ATF_NOFLAGS, 0, offsetof(struct PeriodicContractRequest, contract_number),
		(ASN_TAG_CLASS_CONTEXT | (0 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_ContractNumber,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"contract-number"
		},
	{ ATF_NOFLAGS, 0, offsetof(struct PeriodicContractRequest, reporting_rate),
		(ASN_TAG_CLASS_CONTEXT | (1 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_ReportingRate,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"reporting-rate"
		},
	{ ATF_POINTER, 9, offsetof(struct PeriodicContractRequest, projected_profile_modulus),
		(ASN_TAG_CLASS_CONTEXT | (2 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_ProjectedProfileModulus,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"projected-profile-modulus"
		},
	{ ATF_POINTER, 8, offsetof(struct PeriodicContractRequest, ground_vector_modulus),
		(ASN_TAG_CLASS_CONTEXT | (3 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_GroundVectorModulus,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"ground-vector-modulus"
		},
	{ ATF_POINTER, 7, offsetof(struct PeriodicContractRequest, air_vector_modulus),
		(ASN_TAG_CLASS_CONTEXT | (4 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_AirVectorModulus,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"air-vector-modulus"
		},
	{ ATF_POINTER, 6, offsetof(struct PeriodicContractRequest, met_info_modulus),
		(ASN_TAG_CLASS_CONTEXT | (5 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_MetInfoModulus,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"met-info-modulus"
		},
	{ ATF_POINTER, 5, offsetof(struct PeriodicContractRequest, extended_projected_profile_modulus),
		(ASN_TAG_CLASS_CONTEXT | (6 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_ExtendedProjectedProfileModulus,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"extended-projected-profile-modulus"
		},
	{ ATF_POINTER, 4, offsetof(struct PeriodicContractRequest, toa_range_modulus),
		(ASN_TAG_CLASS_CONTEXT | (7 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_TOARangeRequestModulus,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"toa-range-modulus"
		},
	{ ATF_POINTER, 3, offsetof(struct PeriodicContractRequest, speed_schedule_profile_modulus),
		(ASN_TAG_CLASS_CONTEXT | (8 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_SpeedScheduleProfileModulus,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"speed-schedule-profile-modulus"
		},
	{ ATF_POINTER, 2, offsetof(struct PeriodicContractRequest, rnp_profile_modulus),
		(ASN_TAG_CLASS_CONTEXT | (9 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_RNPProfileModulus,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"rnp-profile-modulus"
		},
	{ ATF_POINTER, 1, offsetof(struct PeriodicContractRequest, planned_final_approach_speed_modulus),
		(ASN_TAG_CLASS_CONTEXT | (10 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_PlannedFinalAppSpeedModulus,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"planned-final-approach-speed-modulus"
		},
};
static const int asn_MAP_PeriodicContractRequest_oms_1[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10 };
static const ber_tlv_tag_t asn_DEF_PeriodicContractRequest_tags_1[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (16 << 2))
};
static const asn_TYPE_tag2member_t asn_MAP_PeriodicContractRequest_tag2el_1[] = {
    { (ASN_TAG_CLASS_CONTEXT | (0 << 2)), 0, 0, 0 }, /* contract-number */
    { (ASN_TAG_CLASS_CONTEXT | (1 << 2)), 1, 0, 0 }, /* reporting-rate */
    { (ASN_TAG_CLASS_CONTEXT | (2 << 2)), 2, 0, 0 }, /* projected-profile-modulus */
    { (ASN_TAG_CLASS_CONTEXT | (3 << 2)), 3, 0, 0 }, /* ground-vector-modulus */
    { (ASN_TAG_CLASS_CONTEXT | (4 << 2)), 4, 0, 0 }, /* air-vector-modulus */
    { (ASN_TAG_CLASS_CONTEXT | (5 << 2)), 5, 0, 0 }, /* met-info-modulus */
    { (ASN_TAG_CLASS_CONTEXT | (6 << 2)), 6, 0, 0 }, /* extended-projected-profile-modulus */
    { (ASN_TAG_CLASS_CONTEXT | (7 << 2)), 7, 0, 0 }, /* toa-range-modulus */
    { (ASN_TAG_CLASS_CONTEXT | (8 << 2)), 8, 0, 0 }, /* speed-schedule-profile-modulus */
    { (ASN_TAG_CLASS_CONTEXT | (9 << 2)), 9, 0, 0 }, /* rnp-profile-modulus */
    { (ASN_TAG_CLASS_CONTEXT | (10 << 2)), 10, 0, 0 } /* planned-final-approach-speed-modulus */
};
static asn_SEQUENCE_specifics_t asn_SPC_PeriodicContractRequest_specs_1 = {
	sizeof(struct PeriodicContractRequest),
	offsetof(struct PeriodicContractRequest, _asn_ctx),
	asn_MAP_PeriodicContractRequest_tag2el_1,
	11,	/* Count of tags in the map */
	asn_MAP_PeriodicContractRequest_oms_1,	/* Optional members */
	7, 2,	/* Root/Additions */
	8,	/* Start extensions */
	12	/* Stop extensions */
};
asn_TYPE_descriptor_t asn_DEF_PeriodicContractRequest = {
	"PeriodicContractRequest",
	"PeriodicContractRequest",
	SEQUENCE_free,
	SEQUENCE_print,
	SEQUENCE_constraint,
	SEQUENCE_decode_ber,
	SEQUENCE_encode_der,
	SEQUENCE_decode_xer,
	SEQUENCE_encode_xer,
	SEQUENCE_decode_uper,
	SEQUENCE_encode_uper,
	0,	/* Use generic outmost tag fetcher */
	asn_DEF_PeriodicContractRequest_tags_1,
	sizeof(asn_DEF_PeriodicContractRequest_tags_1)
		/sizeof(asn_DEF_PeriodicContractRequest_tags_1[0]), /* 1 */
	asn_DEF_PeriodicContractRequest_tags_1,	/* Same as above */
	sizeof(asn_DEF_PeriodicContractRequest_tags_1)
		/sizeof(asn_DEF_PeriodicContractRequest_tags_1[0]), /* 1 */
	0,	/* No PER visible constraints */
	asn_MBR_PeriodicContractRequest_1,
	11,	/* Elements count */
	&asn_SPC_PeriodicContractRequest_specs_1	/* Additional specs */
};
