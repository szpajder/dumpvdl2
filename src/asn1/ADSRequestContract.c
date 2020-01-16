/*
 * Generated by asn1c-0.9.28 (http://lionet.info/asn1c)
 * From ASN.1 module "ADSMessageSetVersion2"
 * 	found in "../../../dumpvdl2.asn1/atn-b2_adsc_v2.asn1"
 * 	`asn1c -fcompound-names -fincludes-quoted -gen-PER`
 */

#include "ADSRequestContract.h"

static asn_per_constraints_t asn_PER_type_ADSRequestContract_constr_1 GCC_NOTUSED = {
	{ APC_CONSTRAINED,	 2,  2,  0,  2 }	/* (0..2) */,
	{ APC_UNCONSTRAINED,	-1, -1,  0,  0 },
	0, 0	/* No PER value map */
};
static asn_TYPE_member_t asn_MBR_ADSRequestContract_1[] = {
	{ ATF_NOFLAGS, 0, offsetof(struct ADSRequestContract, choice.demand_contract),
		(ASN_TAG_CLASS_CONTEXT | (0 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_DemandContractRequest,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"demand-contract"
		},
	{ ATF_NOFLAGS, 0, offsetof(struct ADSRequestContract, choice.event_contract),
		(ASN_TAG_CLASS_CONTEXT | (1 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_EventContractRequest,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"event-contract"
		},
	{ ATF_NOFLAGS, 0, offsetof(struct ADSRequestContract, choice.periodic_contract),
		(ASN_TAG_CLASS_CONTEXT | (2 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_PeriodicContractRequest,
		0,	/* Defer constraints checking to the member type */
		0,	/* No PER visible constraints */
		0,
		"periodic-contract"
		},
};
static const asn_TYPE_tag2member_t asn_MAP_ADSRequestContract_tag2el_1[] = {
    { (ASN_TAG_CLASS_CONTEXT | (0 << 2)), 0, 0, 0 }, /* demand-contract */
    { (ASN_TAG_CLASS_CONTEXT | (1 << 2)), 1, 0, 0 }, /* event-contract */
    { (ASN_TAG_CLASS_CONTEXT | (2 << 2)), 2, 0, 0 } /* periodic-contract */
};
static asn_CHOICE_specifics_t asn_SPC_ADSRequestContract_specs_1 = {
	sizeof(struct ADSRequestContract),
	offsetof(struct ADSRequestContract, _asn_ctx),
	offsetof(struct ADSRequestContract, present),
	sizeof(((struct ADSRequestContract *)0)->present),
	asn_MAP_ADSRequestContract_tag2el_1,
	3,	/* Count of tags in the map */
	0,
	-1	/* Extensions start */
};
asn_TYPE_descriptor_t asn_DEF_ADSRequestContract = {
	"ADSRequestContract",
	"ADSRequestContract",
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
	&asn_PER_type_ADSRequestContract_constr_1,
	asn_MBR_ADSRequestContract_1,
	3,	/* Elements count */
	&asn_SPC_ADSRequestContract_specs_1	/* Additional specs */
};

