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
#include <stdlib.h>
#include <libacars/list.h>		// la_list
#include <libacars/vstring.h>		// la_vstring
#include "dumpvdl2.h"			// XCALLOC, XFREE
#include "tlv.h"

// Forward declarations
tlv_type_descriptor_t tlv_DEF_unknown_tag;
tlv_type_descriptor_t tlv_DEF_unparseable_tag;

static void tlv_tag_destroy(void *tag) {
	if(tag == NULL) {
		return;
	}
	CAST_PTR(t, tlv_tag_t *, tag);
	if(t->td != NULL) {
		if(t->td->destroy != NULL) {
			t->td->destroy(t->data);
		} else {
			XFREE(t->data);
		}
	}
	XFREE(tag);
}

void tlv_list_destroy(la_list *p) {
	la_list_free_full(p, tlv_tag_destroy);
	p = NULL;
}

la_list *tlv_list_append(la_list *head, uint8_t typecode, tlv_type_descriptor_t *td, uint8_t *data) {
	tlv_tag_t *tag = XCALLOC(1, sizeof(tlv_tag_t));
	tag->typecode = typecode;
	tag->td = td;
	tag->data = data;
	return la_list_append(head, tag);
}

tlv_tag_t *tlv_list_search(la_list *ptr, uint8_t const typecode) {
	while(ptr != NULL) {
		if(ptr->data != NULL) {
			CAST_PTR(tag, tlv_tag_t *, ptr->data);
			if(tag->typecode == typecode) {
				return tag;
			}
		}
		ptr = ptr->next;
	}
	return NULL;
}

la_list *tlv_single_tag_parse(uint8_t typecode, uint8_t *buf, size_t tag_len, dict const *tag_table, la_list *list) {
	ASSERT(buf != NULL);
	ASSERT(tag_table != NULL);

	CAST_PTR(td, tlv_type_descriptor_t *, dict_search(tag_table, (int)typecode));
	if(td == NULL) {
		debug_print("Unknown type code %u\n", typecode);
		td = &tlv_DEF_unknown_tag;
	}
	ASSERT(td->parse != NULL);
	void *parsed = NULL;
reparse:
	parsed = td->parse(typecode, buf, tag_len);
	if(parsed == NULL) {
		td = &tlv_DEF_unparseable_tag;
// tlv_unparseable_tag_parse() does not return NULL, so we don't expect a loop here
		goto reparse;
	}
	return tlv_list_append(list, typecode, td, parsed);
}

la_list *tlv_parse(uint8_t *buf, size_t len, dict const *tag_table, size_t const len_octets) {
	ASSERT(buf != NULL);
	ASSERT(tag_table != NULL);
	la_list *head = NULL;
	uint8_t *ptr = buf;
	size_t tlv_min_tag_len = 1 + len_octets;	/* type code + <len_octets> length field + empty data field */
	size_t tag_len;
	while(len >= tlv_min_tag_len) {
		uint8_t typecode = *ptr;
		ptr++; len--;

		tag_len = (size_t)(*ptr);
		if(len_octets == 2) {
			tag_len = (tag_len << 8) | (size_t)ptr[1];
		}

		ptr += len_octets; len -= len_octets;
		if(tag_len > len) {
			debug_print("TLV param %02x truncated: tag_len=%zu buflen=%zu\n", typecode, tag_len, len);
			return NULL;
		} else if(UNLIKELY(tag_len == 0)) {
			debug_print("TLV param %02x: bad length 0\n", typecode);
			return NULL;
		}
		head = tlv_single_tag_parse(typecode, ptr, tag_len, tag_table, head);
		ptr += tag_len; len -= tag_len;
	}
	if(len > 0) {
		debug_print("Warning: %zu unparsed octets left at end of TLV list\n", len);
	}
	return head;
}

static void tlv_tag_output_text(void const * const p, void *ctx) {
	ASSERT(p);
	ASSERT(ctx);

	CAST_PTR(t, tlv_tag_t *, p);
	ASSERT(t->td != NULL);
	if(t->td->format_text != NULL) {
		t->td->format_text(ctx, t->td->label, t->data);
	}
}

void tlv_list_format_text(la_vstring * const vstr, la_list *tlv_list, int indent) {
	ASSERT(vstr);
	ASSERT(indent >= 0);
	if(tlv_list == NULL) {
		return;
	}
	tlv_formatter_ctx_t ctx = {
		.vstr = vstr,
		.indent = indent
	};
	la_list_foreach(tlv_list, tlv_tag_output_text, &ctx);
}

// Parsers and formatters for common data types

TLV_PARSER(tlv_octet_string_parse) {
	UNUSED(typecode);
	return octet_string_new(buf, len);
}

TLV_FORMATTER(tlv_octet_string_format_text) {
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: ", label);
	octet_string_format_text(ctx->vstr, data, 0);
	EOL(ctx->vstr);
}

TLV_FORMATTER(tlv_octet_string_with_ascii_format_text) {
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: ", label);
	octet_string_with_ascii_format_text(ctx->vstr, data, 0);
	EOL(ctx->vstr);
}

TLV_FORMATTER(tlv_octet_string_as_ascii_format_text) {
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: ", label);
	octet_string_as_ascii_format_text(ctx->vstr, data, 0);
	EOL(ctx->vstr);
}

TLV_PARSER(tlv_uint8_parse) {
	UNUSED(typecode);
	if(len < sizeof(uint8_t)) {
		return NULL;
	}
	return UINT_TO_PTR(buf[0]);
}

TLV_PARSER(tlv_uint16_msbfirst_parse) {
	UNUSED(typecode);
	if(len < sizeof(uint16_t)) {
		return NULL;
	}
	return UINT_TO_PTR(extract_uint16_msbfirst(buf));
}

TLV_PARSER(tlv_uint32_msbfirst_parse) {
	UNUSED(typecode);
	if(len < sizeof(uint32_t)) {
		return NULL;
	}
	return UINT_TO_PTR(extract_uint32_msbfirst(buf));
}

TLV_FORMATTER(tlv_uint_format_text) {
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: %lu\n",
		label, PTR_TO_UINT(data));
}

typedef struct {
	uint8_t typecode;
	octet_string_t *data;
} tlv_unparsed_tag_t;

TLV_PARSER(tlv_unknown_tag_parse) {
	tlv_unparsed_tag_t *t = XCALLOC(1, sizeof(tlv_unparsed_tag_t));
	t->typecode = typecode;
	t->data = octet_string_new(buf, len);
	return t;
}

TLV_FORMATTER(tlv_unknown_tag_format_text) {
	UNUSED(label);
	CAST_PTR(t, tlv_unparsed_tag_t *, data);
	LA_ISPRINTF(ctx->vstr, ctx->indent, "-- Unknown TLV (code: 0x%02x): ", t->typecode);
	octet_string_format_text(ctx->vstr, t->data, 0);
	EOL(ctx->vstr);
}

TLV_DESTRUCTOR(tlv_unknown_tag_destroy) {
	if(data == NULL) {
		return;
	}
	CAST_PTR(t, tlv_unparsed_tag_t *, data);
	XFREE(t->data);
	XFREE(t);
}

// A no-op destructor
// Used for uints stored in pointers to prevent the default
// destructor from running (it performs free() which would
// cause a segfault on a fake pointer value)
TLV_DESTRUCTOR(tlv_destroy_noop) {
	UNUSED(data);
	// no-op
}

tlv_type_descriptor_t tlv_DEF_unknown_tag = {
	.label = "Unknown tag",
	.json_key = NULL,
	.parse = tlv_unknown_tag_parse,
	.format_text = tlv_unknown_tag_format_text,
	.format_json = NULL,
	.destroy = tlv_unknown_tag_destroy
};

TLV_FORMATTER(tlv_unparseable_tag_format_text) {
	UNUSED(label);
	CAST_PTR(t, tlv_unparsed_tag_t *, data);
	LA_ISPRINTF(ctx->vstr, ctx->indent, "-- Unparseable TLV (code: 0x%02x): ", t->typecode);
	octet_string_format_text(ctx->vstr, t->data, 0);
	EOL(ctx->vstr);
}

tlv_type_descriptor_t tlv_DEF_unparseable_tag = {
	.label = "Unparseable tag",
	.json_key = NULL,
	.parse = tlv_unknown_tag_parse,
	.format_text = tlv_unparseable_tag_format_text,
	.format_json = NULL,
	.destroy = tlv_unknown_tag_destroy
};
