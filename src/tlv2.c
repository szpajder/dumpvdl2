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
#include "tlv2.h"

// Forward declarations
tlv2_type_descriptor_t tlv2_DEF_unknown_tag;
tlv2_type_descriptor_t tlv2_DEF_unparseable_tag;
tlv2_type_descriptor_t tlv2_DEF_unknown_tag;

static void tlv2_tag_destroy(void *tag) {
	if(tag == NULL) {
		return;
	}
	CAST_PTR(t, tlv2_tag_t *, tag);
	if(t->td != NULL) {
		if(t->td->destroy != NULL) {
			t->td->destroy(t->data);
		} else {
			XFREE(t->data);
		}
	}
	XFREE(tag);
}

void tlv2_list_destroy(la_list *p) {
	la_list_free_full(p, tlv2_tag_destroy);
	p = NULL;
}

la_list *tlv2_list_append(la_list *head, uint8_t typecode, tlv2_type_descriptor_t *td, uint8_t *data) {
	tlv2_tag_t *tag = XCALLOC(1, sizeof(tlv2_tag_t));
	tag->typecode = typecode;
	tag->td = td;
	tag->data = data;
	return la_list_append(head, tag);
}

la_list *tlv2_list_search(la_list *ptr, uint8_t const typecode) {
	while(ptr != NULL) {
		if(ptr->data != NULL) {
			CAST_PTR(tag, tlv2_tag_t *, ptr->data);
			if(tag->typecode == typecode) break;
		}
		ptr = ptr->next;
	}
	return ptr;
}
// FIXME: wszystko na size_t
la_list *tlv2_parse(uint8_t *buf, uint16_t len, dict const *tag_table, uint8_t len_octets) {
	ASSERT(buf != NULL);
	ASSERT(tag_table != NULL);
	la_list *head = NULL;
	uint8_t *ptr = buf;
	uint8_t tlv_min_datalen = 1 + len_octets;	/* type code + <len_octets> length field + empty data field */
	uint16_t datalen;
	while(len >= tlv_min_datalen) {
		uint8_t typecode = *ptr;
		ptr++; len--;

		datalen = *ptr;
		if(len_octets == 2) {
			datalen = (datalen << 8) | (uint16_t)ptr[1];
		}

		ptr += len_octets; len -= len_octets;
		if(datalen > len) {
			debug_print("TLV param %02x truncated: datalen=%u buflen=%u\n", typecode, datalen, len);
			return NULL;
		}
		CAST_PTR(td, tlv2_type_descriptor_t *, dict_search(tag_table, (int)typecode));
		if(td == NULL) {
			debug_print("Unknown type code %u\n", typecode);
			td = &tlv2_DEF_unknown_tag;
		}
		ASSERT(td->parse != NULL);
		void *parsed = NULL;
reparse:
		parsed = td->parse(typecode, ptr, datalen);
		if(parsed == NULL) {
			td = &tlv2_DEF_unparseable_tag;
// tlv2_unparseable_tag_parse() does not return NULL, so we don't expect a loop here
			goto reparse;
		}
		head = tlv2_list_append(head, typecode, td, parsed);
		ptr += datalen; len -= datalen;
	}
	if(len > 0) {
		debug_print("Warning: %u unparsed octets left at end of TLV list\n", len);
	}
	return head;
}

static void tlv2_tag_output_text(void const * const p, void *ctx) {
	ASSERT(p);
	ASSERT(ctx);

	CAST_PTR(t, tlv2_tag_t *, p);
	ASSERT(t->td != NULL);
	if(t->td->format_text != NULL) {
		t->td->format_text(ctx, t->td->label, t->data);
	}
}

void tlv2_list_format_text(la_vstring * const vstr, la_list *tlv_list, int indent) {
	ASSERT(vstr);
	ASSERT(indent >= 0);
	if(tlv_list == NULL) {
		return;
	}
	tlv2_formatter_ctx_t ctx = {
		.vstr = vstr,
		.indent = indent
	};
	la_list_foreach(tlv_list, tlv2_tag_output_text, &ctx);
}

// Parsers and formatters for common data types

TLV2_PARSER(tlv2_octet_string_parse) {
// -Wunused-parameter
	(void)typecode;
	return octet_string_new(buf, len);
}

TLV2_FORMATTER(tlv2_octet_string_format_text) {
	LA_ISPRINTF(ctx->vstr, ctx->indent, "%s: ", label);
	octet_string_format_text(ctx->vstr, data, 0);
}

typedef struct {
	uint8_t typecode;
	octet_string_t *data;
} tlv2_unparsed_tag_t;

TLV2_PARSER(tlv2_unknown_tag_parse) {
	tlv2_unparsed_tag_t *t = XCALLOC(1, sizeof(tlv2_unparsed_tag_t));
	t->typecode = typecode;
	t->data = octet_string_new(buf, len);
	return t;
}

TLV2_FORMATTER(tlv2_unknown_tag_format_text) {
// -Wunused-parameter
	(void)label;
	CAST_PTR(t, tlv2_unparsed_tag_t *, data);
	LA_ISPRINTF(ctx->vstr, ctx->indent, "-- Unknown TLV (code: 0x%02x): ", t->typecode);
	octet_string_format_text(ctx->vstr, t->data, 0);
}

TLV2_DESTRUCTOR(tlv2_unknown_tag_destroy) {
	if(data == NULL) {
		return;
	}
	CAST_PTR(t, tlv2_unparsed_tag_t *, data);
	XFREE(t->data);
	XFREE(t);
}

tlv2_type_descriptor_t tlv2_DEF_unknown_tag = {
	.label = "Unknown tag",
	.json_key = NULL,
	.parse = tlv2_unknown_tag_parse,
	.format_text = tlv2_unknown_tag_format_text,
	.format_json = NULL,
	.destroy = tlv2_unknown_tag_destroy
};

TLV2_FORMATTER(tlv2_unparseable_tag_format_text) {
// -Wunused-parameter
	(void)label;
	CAST_PTR(t, tlv2_unparsed_tag_t *, data);
	LA_ISPRINTF(ctx->vstr, ctx->indent, "-- Unparseable TLV (code: 0x%02x): ", t->typecode);
	octet_string_format_text(ctx->vstr, t->data, 0);
}

tlv2_type_descriptor_t tlv2_DEF_unparseable_tag = {
	.label = "Unparseable tag",
	.json_key = NULL,
	.parse = tlv2_unknown_tag_parse,
	.format_text = tlv2_unparseable_tag_format_text,
	.format_json = NULL,
	.destroy = tlv2_unknown_tag_destroy
};
