/*
 * Generated by asn1c-0.9.28 (http://lionet.info/asn1c)
 * From ASN.1 module "ADSMessageSetVersion2"
 * 	found in "../../../dumpvdl2.asn1/atn-b2_adsc_v2.asn1"
 * 	`asn1c -fcompound-names -fincludes-quoted -gen-PER`
 */

#include "EPPTolerancesValues.h"

static asn_TYPE_member_t asn_MBR_EPPTolerancesValues_1[] = {
	{ ATF_POINTER, 4, offsetof(struct EPPTolerancesValues, greatdistancecircle),
		(ASN_TAG_CLASS_CONTEXT | (0 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_EPPTolGCDistance,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"greatdistancecircle"
		},
	{ ATF_POINTER, 3, offsetof(struct EPPTolerancesValues, level),
		(ASN_TAG_CLASS_CONTEXT | (1 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_EPPTolLevel,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"level"
		},
	{ ATF_POINTER, 2, offsetof(struct EPPTolerancesValues, eta),
		(ASN_TAG_CLASS_CONTEXT | (2 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_EPPTolETA,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"eta"
		},
	{ ATF_POINTER, 1, offsetof(struct EPPTolerancesValues, air_speed),
		(ASN_TAG_CLASS_CONTEXT | (3 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_AirspeedChangeTolerance,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"air-speed"
		},
};
static const int asn_MAP_EPPTolerancesValues_oms_1[] = { 0, 1, 2, 3 };
static const ber_tlv_tag_t asn_DEF_EPPTolerancesValues_tags_1[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (16 << 2))
};
static const asn_TYPE_tag2member_t asn_MAP_EPPTolerancesValues_tag2el_1[] = {
    { (ASN_TAG_CLASS_CONTEXT | (0 << 2)), 0, 0, 0 }, /* greatdistancecircle */
    { (ASN_TAG_CLASS_CONTEXT | (1 << 2)), 1, 0, 0 }, /* level */
    { (ASN_TAG_CLASS_CONTEXT | (2 << 2)), 2, 0, 0 }, /* eta */
    { (ASN_TAG_CLASS_CONTEXT | (3 << 2)), 3, 0, 0 } /* air-speed */
};
static asn_SEQUENCE_specifics_t asn_SPC_EPPTolerancesValues_specs_1 = {
	sizeof(struct EPPTolerancesValues),
	offsetof(struct EPPTolerancesValues, _asn_ctx),
	asn_MAP_EPPTolerancesValues_tag2el_1,
	4,	/* Count of tags in the map */
	asn_MAP_EPPTolerancesValues_oms_1,	/* Optional members */
	4, 0,	/* Root/Additions */
	3,	/* Start extensions */
	5	/* Stop extensions */
};
asn_TYPE_descriptor_t asn_DEF_EPPTolerancesValues = {
	"EPPTolerancesValues",
	"EPPTolerancesValues",
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
	asn_DEF_EPPTolerancesValues_tags_1,
	sizeof(asn_DEF_EPPTolerancesValues_tags_1)
		/sizeof(asn_DEF_EPPTolerancesValues_tags_1[0]), /* 1 */
	asn_DEF_EPPTolerancesValues_tags_1,	/* Same as above */
	sizeof(asn_DEF_EPPTolerancesValues_tags_1)
		/sizeof(asn_DEF_EPPTolerancesValues_tags_1[0]), /* 1 */
	0,	/* No PER visible constraints */
	asn_MBR_EPPTolerancesValues_1,
	4,	/* Elements count */
	&asn_SPC_EPPTolerancesValues_specs_1	/* Additional specs */
};

