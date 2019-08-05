/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
 *  Copyright (c) 2017-2019 Tomasz Lemiech <szpajder@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "libacars/list.h"		// la_list
#include "dumpvdl2.h"			// dict
#include "tlv.h"
#include "config.h"			// IS_BIG_ENDIAN
#include "atn.h"			// ATN_TRAFFIC_TYPES_ALL, ATSC_TRAFFIC_CLASSES_ALL

typedef struct {
	octet_string_t sec_rid;
	la_list *sec_info;
} atn_sec_label_t;

dict const atn_traffic_types[] = {
	{ .id =  1, .val = "ATS" },
	{ .id =  2, .val = "AOC" },
	{ .id =  4, .val = "ATN Administrative" },
	{ .id =  8, .val = "General Comms" },
	{ .id = 16, .val = "ATN System Mgmt" },
	{ .id =  0, .val = NULL }
};

dict const atsc_traffic_classes[] = {
	{ .id =  1, .val = "A" },
	{ .id =  2, .val = "B" },
	{ .id =  4, .val = "C" },
	{ .id =  8, .val = "D" },
	{ .id = 16, .val = "E" },
	{ .id = 32, .val = "F" },
	{ .id = 64, .val = "G" },
	{ .id =128, .val = "H" },
	{ .id =  0, .val = NULL }
};

typedef enum {
	TT_UNKNOWN,
	TT_ATN_OPER,
	TT_ATN_ADMIN,
	TT_ATN_SYS_MGMT,
} traffic_type;

typedef enum {
	CAT_UNKNOWN,
	CAT_ATSC,
	CAT_AOC,
	CAT_NONE
} traffic_category;

typedef struct {
	traffic_type type;
	traffic_category category;
	uint8_t policy;
} tag_atn_traffic_type_t;

TLV_PARSER(atn_traffic_type_parse) {
	UNUSED(typecode);
	if(len < 1) {
		return NULL;
	}
	NEW(tag_atn_traffic_type_t, t);
	t->type = TT_UNKNOWN;
	t->category = CAT_UNKNOWN;
	t->policy = buf[0] & 0x1f;
	switch(buf[0] >> 5) {
	case 0:
		t->type = TT_ATN_OPER,
		t->category = CAT_ATSC;
		break;
	case 1:
// exception :/
		if(buf[0] == 0x30) {
			t->type = TT_ATN_ADMIN;
			t->category = CAT_NONE;
		} else {
			t->type = TT_ATN_OPER;
			t->category = CAT_AOC;
		}
		break;
	case 3:
		t->type = TT_ATN_SYS_MGMT;
		t->category = CAT_NONE;
		break;
	}
	return t;
}

TLV_FORMATTER(atn_traffic_type_format_text) {
	static dict const traffic_categories[] = {
		{ .id = CAT_ATSC, .val = "ATSC" },
		{ .id = CAT_AOC, .val = "AOC" },
		{ .id = CAT_NONE, .val = "none" },
		{ .id = 0, .val = NULL }
	};
	static dict const traffic_types[] = {
		{ .id = TT_ATN_OPER, .val = "ATN operational" },
		{ .id = TT_ATN_ADMIN, .val = "ATN administrative" },
		{ .id = TT_ATN_SYS_MGMT, .val = "ATN system management" },
		{ .id = 0, .val = NULL }
	};

	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);

	CAST_PTR(t, tag_atn_traffic_type_t *, data);
	CAST_PTR(type, char *, dict_search(traffic_types, t->type));
	CAST_PTR(category, char *, dict_search(traffic_categories, t->category));
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s:\n", label);
	ctx->indent++;
	LA_ISPRINTF(ctx->vstr, ctx->indent, "Type: %s\n", type ? type : "unknown");
	LA_ISPRINTF(ctx->vstr, ctx->indent, "Category: %s\n", category ? category : "unknown");
// TODO: stringify all policies according to 9705, Table 5.6-1
	LA_ISPRINTF(ctx->vstr, ctx->indent, "Route policy: 0x%02x\n", t->policy);
	ctx->indent--;
}

typedef struct {
	uint8_t subnet;
	uint8_t permitted_traffic_types;
} tag_subnet_type_t;

TLV_PARSER(atn_subnet_type_parse) {
	UNUSED(typecode);
	if(len != 2) {
		return NULL;
	}
	NEW(tag_subnet_type_t, t);
	t->subnet = buf[0];
	t->permitted_traffic_types = buf[1];
	return t;
}

TLV_FORMATTER(atn_subnet_type_format_text) {
	static dict const subnet_types[] = {
		{ .id = 1, .val = "Mode S" },
		{ .id = 2, .val = "VDL" },
		{ .id = 3, .val = "AMSS" },
		{ .id = 4, .val = "Gatelink" },
		{ .id = 5, .val = "HF" },
		{ .id = 0, .val = NULL }
	};

	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);

	CAST_PTR(t, tag_subnet_type_t *, data);
	CAST_PTR(subnet, char *, dict_search(subnet_types, t->subnet));
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s:\n", label);
	ctx->indent++;
	LA_ISPRINTF(ctx->vstr, ctx->indent, "Subnet: %s\n", subnet ? subnet : "unknown");
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: ", "Permitted traffic");
	if((t->permitted_traffic_types & ATN_TRAFFIC_TYPES_ALL) == ATN_TRAFFIC_TYPES_ALL) {
		la_vstring_append_sprintf(ctx->vstr, "%s", "all");
	} else {
		bitfield_format_text(ctx->vstr, t->permitted_traffic_types, atn_traffic_types);
	}
	EOL(ctx->vstr);
	ctx->indent--;
}

TLV_FORMATTER(atn_supported_traffic_classes_format_text) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);

	CAST_PTR(t, uint8_t *, data);
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: ", label);
	if((*t & ATSC_TRAFFIC_CLASSES_ALL) == ATSC_TRAFFIC_CLASSES_ALL) {
		la_vstring_append_sprintf(ctx->vstr, "%s", "all");
	} else {
		bitfield_format_text(ctx->vstr, *t, atsc_traffic_classes);
	}
	EOL(ctx->vstr);
}

TLV_FORMATTER(atn_sec_class_format_text) {
	static dict const security_classes[] = {
		{ .id = 1, .val = "unclassified" },
		{ .id = 2, .val = "restricted" },
		{ .id = 3, .val = "confidential" },
		{ .id = 4, .val = "secret" },
		{ .id = 5, .val = "top secret" },
		{ .id = 0, .val = NULL }
	};
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);

	CAST_PTR(t, uint8_t *, data);
	CAST_PTR(class, char *, dict_search(security_classes, *t));
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: %s\n",
		label, class ? class : "unassigned");
}

dict const atn_security_tags[] = {
	{
		.id = 0x3,
		.val = &(tlv_type_descriptor_t){
			.label = "Security classification",
			.parse = tlv_uint8_parse,
			.format_text = atn_sec_class_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 0x5,
		.val = &(tlv_type_descriptor_t){
			.label = "Subnetwork type",
			.parse = atn_subnet_type_parse,
			.format_text = atn_subnet_type_format_text,
			.destroy = NULL
		}
	},
// Supported ATSC classes TLV uses a typecode of 6 or 7,
// depending on whether non-ATSC traffic is allowed on the route or not.
	{
		.id = 0x6,
		.val = &(tlv_type_descriptor_t){
			.label = "Supported ATSC classes",
			.parse = tlv_uint8_parse,
			.format_text = atn_supported_traffic_classes_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 0x7,
		.val = &(tlv_type_descriptor_t){
			.label = "Supported ATSC classes",
			.parse = tlv_uint8_parse,
			.format_text = atn_supported_traffic_classes_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 0xf,
		.val = &(tlv_type_descriptor_t){
			.label = "Traffic type",
			.parse = atn_traffic_type_parse,
			.format_text = atn_traffic_type_format_text,
			.destroy = NULL
		}
	},
	{
		.id = 0x0,
		.val = NULL
	}
};

static la_list *atn_sec_info_parse(uint8_t *buf, size_t len) {
	ASSERT(buf != NULL);
	la_list *l = NULL;
// In ATN all security tag names have a length of 1, hence we may
// treat the single-byte name as a dict index and parse the whole
// tag set as TLV. If we encounter a tag name length other than 1,
// this method won't work, so we return NULL to indicate parsing error.
	while(len >= 3) {	// tag set name len + tag set name + tag set len + 0-length sec tag
		if(buf[0] != 1) {
			debug_print("Unsupported tag set name length %u\n", buf[0]);
			goto fail;
		}
		buf++; len--;
		uint8_t tagset_name = buf[0];
		uint8_t tagset_len = buf[1];
		buf += 2; len -= 2;
		if(len < tagset_len) {
			debug_print("tagset 0x%02u truncated: len %zu < tagset_len %u\n",
				tagset_name, len, tagset_len);
			goto fail;
		}
		l = tlv_single_tag_parse(tagset_name, buf, tagset_len, atn_security_tags, l);
		buf += tagset_len; len -= tagset_len;
	}
	if(len > 0) {
		debug_print("%zu octets left after parsing sec_info\n", len);
		goto fail;
	}
	return l;
fail:
	tlv_list_destroy(l);
	return NULL;
}


TLV_PARSER(atn_sec_label_parse) {
	UNUSED(typecode);

	if(len < 1) {
		return NULL;
	}
	uint8_t srid_len = buf[0];
	buf++; len--;
	if(len < srid_len) {
		debug_print("srid truncated: buf len %zu < srid_len %u\n", len, srid_len);
		return NULL;
	}
	NEW(atn_sec_label_t, l);
	l->sec_rid.buf = buf;
	l->sec_rid.len = srid_len;
	buf += srid_len; len -= srid_len;

	if(len < 1) {
		debug_print("sinfo absent\n");
		goto end;
	}
	uint8_t sinfo_len = buf[0];
	buf++; len--;
	if(len < 1) {
		debug_print("sinfo present but length 0\n");
		goto end;
	}
	if(len < sinfo_len) {
		debug_print("sinfo truncated: buf len %zu < sinfo_len %u\n", len, sinfo_len);
		goto fail;
	}
	l->sec_info = atn_sec_info_parse(buf, len);
	if(l->sec_info == NULL) {
		goto fail;
	}
end:
	return l;
fail:
	XFREE(l);
	return NULL;
}

TLV_FORMATTER(atn_sec_label_format_text) {
	ASSERT(ctx != NULL);
	ASSERT(ctx->vstr != NULL);
	ASSERT(ctx->indent >= 0);

	CAST_PTR(l, atn_sec_label_t *, data);
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s:\n", label);
	LA_ISPRINTF(ctx->vstr, ctx->indent+1, "%s: ", "Reg ID");
	octet_string_format_text(ctx->vstr, &l->sec_rid, 0);
	EOL(ctx->vstr);
	if(l->sec_info == NULL) {
		return;
	}
	LA_ISPRINTF(ctx->vstr, ctx->indent+1, "%s:\n", "Info");
	tlv_list_format_text(ctx->vstr, l->sec_info, ctx->indent+2);
}

TLV_DESTRUCTOR(atn_sec_label_destroy) {
	if(data == NULL) {
		return;
	}
	CAST_PTR(l, atn_sec_label_t *, data);
	tlv_list_destroy(l->sec_info);
	XFREE(data);
}
