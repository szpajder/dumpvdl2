/*
 * Generated by asn1c-0.9.28 (http://lionet.info/asn1c)
 * From ASN.1 module "ACSE-1"
 * 	found in "atn-ulcs.asn1"
 * 	`asn1c -fcompound-names -fincludes-quoted -gen-PER`
 */

#include "AP-title.h"

static asn_per_constraints_t asn_PER_type_AP_title_constr_1 GCC_NOTUSED = {
	{ APC_CONSTRAINED | APC_EXTENSIBLE,  1,  1,  0,  1 }	/* (0..1,...) */,
	{ APC_UNCONSTRAINED,	-1, -1,  0,  0 },
	0, 0	/* No PER value map */
};
static asn_TYPE_member_t asn_MBR_AP_title_1[] = {
	{ ATF_NOFLAGS, 0, offsetof(struct AP_title, choice.ap_title_form2),
		(ASN_TAG_CLASS_UNIVERSAL | (6 << 2)),
		0,
		&asn_DEF_AP_title_form2,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"ap-title-form2"
		},
	{ ATF_NOFLAGS, 0, offsetof(struct AP_title, choice.ap_title_form1),
		-1 /* Ambiguous tag (CHOICE?) */,
		0,
		&asn_DEF_AP_title_form1,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"ap-title-form1"
		},
};
static const asn_TYPE_tag2member_t asn_MAP_AP_title_tag2el_1[] = {
    { (ASN_TAG_CLASS_UNIVERSAL | (6 << 2)), 0, 0, 0 }, /* ap-title-form2 */
    { (ASN_TAG_CLASS_UNIVERSAL | (16 << 2)), 1, 0, 0 } /* rdnSequence */
};
static asn_CHOICE_specifics_t asn_SPC_AP_title_specs_1 = {
	sizeof(struct AP_title),
	offsetof(struct AP_title, _asn_ctx),
	offsetof(struct AP_title, present),
	sizeof(((struct AP_title *)0)->present),
	asn_MAP_AP_title_tag2el_1,
	2,	/* Count of tags in the map */
	0,
	2	/* Extensions start */
};
asn_TYPE_descriptor_t asn_DEF_AP_title = {
	"AP-title",
	"AP-title",
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
	&asn_PER_type_AP_title_constr_1,
	asn_MBR_AP_title_1,
	2,	/* Elements count */
	&asn_SPC_AP_title_specs_1	/* Additional specs */
};
