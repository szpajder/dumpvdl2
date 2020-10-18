/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2020 Tomasz Lemiech <szpajder@gmail.com>
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
#include <stdio.h>
#include <stdarg.h>                 // va_list, etc
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <libacars/libacars.h>      // la_proto_node, la_type_descriptor
#include <libacars/vstring.h>       // la_vstring, la_isprintf_multiline_text()
#include "dumpvdl2.h"
#include "libacars/json.h"

void *xcalloc(size_t nmemb, size_t size, char const *file, int line, char const *func) {
	void *ptr = calloc(nmemb, size);
	if(ptr == NULL) {
		fprintf(stderr, "%s:%d: %s(): calloc(%zu, %zu) failed: %s\n",
				file, line, func, nmemb, size, strerror(errno));
		_exit(1);
	}
	return ptr;
}

void *xrealloc(void *ptr, size_t size, char const *file, int line, char const *func) {
	ptr = realloc(ptr, size);
	if(ptr == NULL) {
		fprintf(stderr, "%s:%d: %s(): realloc(%zu) failed: %s\n",
				file, line, func, size, strerror(errno));
		_exit(1);
	}
	return ptr;
}

void *dict_search(dict const *list, int id) {
	if(list == NULL) return NULL;
	dict *ptr;
	for(ptr = (dict *)list; ; ptr++) {
		if(ptr->val == NULL) return NULL;
		if(ptr->id == id) return ptr->val;
	}
}

static char *fmt_hexstring(octet_string_t const *ostring) {
	static const char hex[] = "0123456789abcdef";
	ASSERT(ostring != NULL);
	char *buf = NULL;
	if(ostring->len == 0) {
		return strdup("none");
	}
	buf = XCALLOC(3 * ostring->len + 1, sizeof(char));
	char *ptr = buf;
	uint8_t *ibuf = ostring->buf;
	for(uint16_t i = 0; i < ostring->len; i++) {
		*ptr++ = hex[((ibuf[i] >> 4) & 0xf)];
		*ptr++ = hex[ibuf[i] & 0xf];
		*ptr++ = ' ';
	}
	if(ptr != buf) {
		ptr[-1] = '\0';         // trim trailing space
	}
	return buf;
}

static char *replace_nonprintable_chars(octet_string_t const *ostring) {
	ASSERT(ostring != NULL);
	if(ostring->len == 0) {
		return strdup("");
	}
	char *buf = XCALLOC(ostring->len + 1, sizeof(char));
	char *ptr = buf;
	uint8_t *ibuf = ostring->buf;
	for(size_t i = 0; i < ostring->len; i++) {
		if(ibuf[i] < 32 || ibuf[i] > 126) {
			*ptr++ = '.';
		} else {
			*ptr++ = ibuf[i];
		}
	}
	*ptr = '\0';
	return buf;
}

void bitfield_format_text(la_vstring *vstr, uint8_t const *buf, size_t len, dict const *d) {
	ASSERT(vstr != NULL);
	ASSERT(d != NULL);
	ASSERT(len <= sizeof(uint32_t));

	uint32_t val = 0;
	for(size_t i = 0; i < len; val = (val << 8) | buf[i++])
		;
	if(val == 0) {
		la_vstring_append_sprintf(vstr, "%s", "none");
		return;
	}
	bool first = true;
	for(dict const *ptr = d; ptr->val != NULL; ptr++) {
		if((val & (uint32_t)ptr->id) == (uint32_t)ptr->id) {
			la_vstring_append_sprintf(vstr, "%s%s",
					(first ? "" : ", "), (char *)ptr->val);
			first = false;
		}
	}
}

void bitfield_format_json(la_vstring *vstr, uint8_t const *buf, size_t len, dict const *d, char const *key) {
	ASSERT(vstr != NULL);
	ASSERT(d != NULL);
	ASSERT(len <= sizeof(uint32_t));

	uint32_t val = 0;
	for(size_t i = 0; i < len; val = (val << 8) | buf[i++])
		;
	la_json_array_start(vstr, key);
	if(val != 0) {
		for(dict const *ptr = d; ptr->val != NULL; ptr++) {
			if((val & (uint32_t)ptr->id) == (uint32_t)ptr->id) {
				la_json_append_string(vstr, NULL, ptr->val);
			}
		}
	}
	la_json_array_end(vstr);
	return;
}

uint32_t extract_uint32_msbfirst(uint8_t const *data) {
	ASSERT(data != NULL);
	return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
		((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

uint16_t extract_uint16_msbfirst(uint8_t const *data) {
	ASSERT(data != NULL);
	return ((uint16_t)data[0] << 8) | (uint16_t)data[1];
}

octet_string_t *octet_string_new(void *buf, size_t len) {
	NEW(octet_string_t, ostring);
	ostring->buf = buf;
	ostring->len = len;
	return ostring;
}

int octet_string_parse(uint8_t *buf, size_t len, octet_string_t *result) {
	ASSERT(buf != NULL);
	if(len == 0) {
		debug_print(D_PROTO, "empty buffer\n");
		return -1;
	}
	uint8_t buflen = *buf++; len--;
	if(len < buflen) {
		debug_print(D_PROTO, "buffer truncated: len %zu < expected %u\n", len, buflen);
		return -1;
	}
	result->buf = buf;
	result->len = buflen;
	return 1 + buflen;  // total number of consumed octets
}

void octet_string_format_text(la_vstring *vstr, octet_string_t const *ostring, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(ostring != NULL);
	ASSERT(indent >= 0);

	char *h = fmt_hexstring(ostring);
	LA_ISPRINTF(vstr, indent, "%s", h);
	XFREE(h);
}

void octet_string_with_ascii_format_text(la_vstring *vstr, octet_string_t const *ostring, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(ostring != NULL);
	ASSERT(indent >= 0);

	char *hex = fmt_hexstring(ostring);
	char *ascii = replace_nonprintable_chars(ostring);
	LA_ISPRINTF(vstr, indent, "%s\t\"%s\"", hex, ascii);
	XFREE(hex);
	XFREE(ascii);
}

void octet_string_as_ascii_format_text(la_vstring *vstr, octet_string_t const *ostring, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(ostring != NULL);
	ASSERT(indent >= 0);

	LA_ISPRINTF(vstr, indent, "%s", "");
	if(ostring->len == 0) {
		return;
	}
	char *replaced = replace_nonprintable_chars(ostring);
	la_vstring_append_sprintf(vstr, "%s", replaced);
	XFREE(replaced);
}

void octet_string_as_ascii_format_json(la_vstring *vstr, char const *key,
		octet_string_t const *ostring) {
	ASSERT(vstr != NULL);
	ASSERT(ostring != NULL);

	char *replaced = replace_nonprintable_chars(ostring);
	la_json_append_string(vstr, key, replaced);
	XFREE(replaced);
}

octet_string_t *octet_string_copy(octet_string_t const *ostring) {
	ASSERT(ostring != NULL);
	NEW(octet_string_t, copy);
	copy->len = ostring->len;
	if(ostring->buf != NULL && ostring->len > 0) {
		copy->buf = XCALLOC(copy->len, sizeof(uint8_t));
		memcpy(copy->buf, ostring->buf, ostring->len);
	}
	return copy;
}

void octet_string_destroy(octet_string_t *ostring) {
	if(ostring == NULL) {
		return;
	}
	XFREE(ostring->buf);
	XFREE(ostring);
}

char *hexdump(uint8_t *data, size_t len) {
	static const char hex[] = "0123456789abcdef";
	if(data == NULL) return strdup("<undef>");
	if(len == 0) return strdup("<none>");

	size_t rows = len / 16;
	if((len & 0xf) != 0) {
		rows++;
	}
	size_t rowlen = 16 * 2 + 16;            // 32 hex digits + 16 spaces per row
	rowlen += 16;                           // ASCII characters per row
	rowlen += 10;                           // extra space for separators
	size_t alloc_size = rows * rowlen + 1;  // terminating NULL
	char *buf = XCALLOC(alloc_size, sizeof(char));
	char *ptr = buf;
	size_t i = 0, j = 0;
	while(i < len) {
		for(j = i; j < i + 16; j++) {
			if(j < len) {
				*ptr++ = hex[((data[j] >> 4) & 0xf)];
				*ptr++ = hex[data[j] & 0xf];
			} else {
				*ptr++ = ' ';
				*ptr++ = ' ';
			}
			*ptr++ = ' ';
			if(j == i + 7) {
				*ptr++ = ' ';
			}
		}
		*ptr++ = ' ';
		*ptr++ = '|';
		for(j = i; j < i + 16; j++) {
			if(j < len) {
				if(data[j] < 32 || data[j] > 126) {
					*ptr++ = '.';
				} else {
					*ptr++ = data[j];
				}
			} else {
				*ptr++ = ' ';
			}
			if(j == i + 7) {
				*ptr++ = ' ';
			}
		}
		*ptr++ = '|';
		*ptr++ = '\n';
		i += 16;
	}
	return buf;
}

void append_hexdump_with_indent(la_vstring *vstr, uint8_t *data, size_t len, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(indent >= 0);
	char *h = hexdump(data, len);
	la_isprintf_multiline_text(vstr, indent, h);
	XFREE(h);
}

// la_proto_node routines for unknown protocols
// which are to be serialized as octet string (hex dump or hex string)

static void unknown_proto_format_text(la_vstring *vstr, void const *data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data != NULL);
	ASSERT(indent >= 0);

	octet_string_t const *ostring = data;
	// fmt_hexstring also checks this conditon, but when it hits, it prints "empty" or "none",
	// which we want to avoid here
	if(ostring->buf == NULL || ostring->len == 0) {
		return;
	}
	LA_ISPRINTF(vstr, indent, "Data (%zu bytes):\n", ostring->len);
	octet_string_format_text(vstr, ostring, indent+1);
	EOL(vstr);
}

static void unknown_proto_format_json(la_vstring *vstr, void const *data) {
	ASSERT(vstr != NULL);
	ASSERT(data != NULL);

	octet_string_t const *ostring = data;
	if(ostring->buf == NULL || ostring->len == 0) {
		return;
	}
	la_json_append_octet_string(vstr, "data", ostring->buf, ostring->len);
}

la_type_descriptor const proto_DEF_unknown = {
	.format_text = unknown_proto_format_text,
	.format_json = unknown_proto_format_json,
	.json_key = "unknown_proto",
	.destroy = NULL
};

la_proto_node *unknown_proto_pdu_new(void *buf, size_t len) {
	octet_string_t *ostring = octet_string_new(buf, len);
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_unknown;
	node->data = ostring;
	node->next = NULL;
	return node;
}
