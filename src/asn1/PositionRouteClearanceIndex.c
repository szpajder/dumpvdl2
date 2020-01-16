/*
 * Generated by asn1c-0.9.28 (http://lionet.info/asn1c)
 * From ASN.1 module "PMCPDLCMessageSetVersion1"
 * 	found in "../../../dumpvdl2.asn1/atn-b1_cpdlc-v1.asn1"
 * 	`asn1c -fcompound-names -fincludes-quoted -gen-PER`
 */

#include "PositionRouteClearanceIndex.h"

static asn_TYPE_member_t asn_MBR_PositionRouteClearanceIndex_1[] = {
	{ ATF_NOFLAGS, 0, offsetof(struct PositionRouteClearanceIndex, position),
		-1 /* Ambiguous tag (CHOICE?) */,
		0,
		&asn_DEF_Position,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"position"
		},
	{ ATF_NOFLAGS, 0, offsetof(struct PositionRouteClearanceIndex, routeClearanceIndex),
		(ASN_TAG_CLASS_UNIVERSAL | (2 << 2)),
		0,
		&asn_DEF_RouteClearanceIndex,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"routeClearanceIndex"
		},
};
static const ber_tlv_tag_t asn_DEF_PositionRouteClearanceIndex_tags_1[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (16 << 2))
};
static const asn_TYPE_tag2member_t asn_MAP_PositionRouteClearanceIndex_tag2el_1[] = {
    { (ASN_TAG_CLASS_UNIVERSAL | (2 << 2)), 1, 0, 0 }, /* routeClearanceIndex */
    { (ASN_TAG_CLASS_CONTEXT | (0 << 2)), 0, 0, 0 }, /* fixName */
    { (ASN_TAG_CLASS_CONTEXT | (1 << 2)), 0, 0, 0 }, /* navaid */
    { (ASN_TAG_CLASS_CONTEXT | (2 << 2)), 0, 0, 0 }, /* airport */
    { (ASN_TAG_CLASS_CONTEXT | (3 << 2)), 0, 0, 0 }, /* latitudeLongitude */
    { (ASN_TAG_CLASS_CONTEXT | (4 << 2)), 0, 0, 0 } /* placeBearingDistance */
};
static asn_SEQUENCE_specifics_t asn_SPC_PositionRouteClearanceIndex_specs_1 = {
	sizeof(struct PositionRouteClearanceIndex),
	offsetof(struct PositionRouteClearanceIndex, _asn_ctx),
	asn_MAP_PositionRouteClearanceIndex_tag2el_1,
	6,	/* Count of tags in the map */
	0, 0, 0,	/* Optional elements (not needed) */
	-1,	/* Start extensions */
	-1	/* Stop extensions */
};
asn_TYPE_descriptor_t asn_DEF_PositionRouteClearanceIndex = {
	"PositionRouteClearanceIndex",
	"PositionRouteClearanceIndex",
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
	asn_DEF_PositionRouteClearanceIndex_tags_1,
	sizeof(asn_DEF_PositionRouteClearanceIndex_tags_1)
		/sizeof(asn_DEF_PositionRouteClearanceIndex_tags_1[0]), /* 1 */
	asn_DEF_PositionRouteClearanceIndex_tags_1,	/* Same as above */
	sizeof(asn_DEF_PositionRouteClearanceIndex_tags_1)
		/sizeof(asn_DEF_PositionRouteClearanceIndex_tags_1[0]), /* 1 */
	0,	/* No PER visible constraints */
	asn_MBR_PositionRouteClearanceIndex_1,
	2,	/* Elements count */
	&asn_SPC_PositionRouteClearanceIndex_specs_1	/* Additional specs */
};

