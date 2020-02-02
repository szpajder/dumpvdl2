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
#include <stdarg.h>	// va_list, etc
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <libacars/libacars.h>		// la_proto_node, la_type_descriptor
#include <libacars/vstring.h>		// la_vstring, la_isprintf_multiline_text()
#include "dumpvdl2.h"

void *xcalloc(size_t nmemb, size_t size, const char *file, const int line, const char *func) {
	void *ptr = calloc(nmemb, size);
	if(ptr == NULL) {
		fprintf(stderr, "%s:%d: %s(): calloc(%zu, %zu) failed: %s\n",
			file, line, func, nmemb, size, strerror(errno));
		_exit(1);
	}
	return ptr;
}

void *xrealloc(void *ptr, size_t size, const char *file, const int line, const char *func) {
	ptr = realloc(ptr, size);
	if(ptr == NULL) {
		fprintf(stderr, "%s:%d: %s(): realloc(%zu) failed: %s\n",
			file, line, func, size, strerror(errno));
		_exit(1);
	}
	return ptr;
}

void *dict_search(const dict *list, int id) {
	if(list == NULL) return NULL;
	dict *ptr;
	for(ptr = (dict *)list; ; ptr++) {
		if(ptr->val == NULL) return NULL;
		if(ptr->id == id) return ptr->val;
	}
}

static char *fmt_hexstring(uint8_t *data, uint16_t len) {
	static const char hex[] = "0123456789abcdef";
	ASSERT(data != NULL);
	char *buf = NULL;
	if(len == 0) {
		return strdup("none");
	}
	buf = XCALLOC(3 * len + 1, sizeof(char));
	char *ptr = buf;
	for(uint16_t i = 0; i < len; i++) {
		*ptr++ = hex[((data[i] >> 4) & 0xf)];
		*ptr++ = hex[data[i] & 0xf];
		*ptr++ = ' ';
	}
	if(ptr != buf)
		ptr[-1] = '\0';		// trim trailing space
	return buf;
}

static char *replace_nonprintable_chars(uint8_t *data, size_t len) {
	ASSERT(data != NULL);
	if(len == 0) {
		return strdup("");
	}
	char *buf = XCALLOC(len + 1, sizeof(char));
	char *ptr = buf;
	for(size_t i = 0; i < len; i++) {
		if(data[i] < 32 || data[i] > 126) {
			*ptr++ = '.';
		} else {
			*ptr++ = data[i];
		}
	}
	*ptr = '\0';
	return buf;
}

void bitfield_format_text(la_vstring *vstr, uint8_t *buf, size_t len, dict const *d) {
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

uint32_t extract_uint32_msbfirst(uint8_t const * const data) {
	ASSERT(data != NULL);
	return	((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
		((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

uint16_t extract_uint16_msbfirst(uint8_t const * const data) {
	ASSERT(data != NULL);
	return	((uint16_t)data[0] << 8) | (uint16_t)data[1];
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
	return 1 + buflen;	// total number of consumed octets
}

void octet_string_format_text(la_vstring * const vstr, void const * const data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data != NULL);
	ASSERT(indent >= 0);

	CAST_PTR(ostring, octet_string_t *, data);
	char *h = fmt_hexstring(ostring->buf, ostring->len);
	LA_ISPRINTF(vstr, indent, "%s", h);
	XFREE(h);
}

void octet_string_with_ascii_format_text(la_vstring * const vstr, void const * const data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data != NULL);
	ASSERT(indent >= 0);

	CAST_PTR(ostring, octet_string_t *, data);
	char *hex = fmt_hexstring(ostring->buf, ostring->len);
	char *ascii = replace_nonprintable_chars(ostring->buf, ostring->len);
	LA_ISPRINTF(vstr, indent, "%s\t\"%s\"", hex, ascii);
	XFREE(hex);
	XFREE(ascii);
}

void octet_string_as_ascii_format_text(la_vstring * const vstr, void const * const data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data != NULL);
	ASSERT(indent >= 0);

	CAST_PTR(ostring, octet_string_t *, data);
	LA_ISPRINTF(vstr, indent, "%s", "");
	if(ostring->len == 0) {
		return;
	}
	char *replaced = replace_nonprintable_chars(ostring->buf, ostring->len);
	la_vstring_append_buffer(vstr, replaced, ostring->len);
	XFREE(replaced);
}

size_t slurp_hexstring(char* string, uint8_t **buf) {
	if(string == NULL)
		return 0;
	size_t slen = strlen(string);
	if(slen & 1)
		slen--;
	size_t dlen = slen / 2;
	if(dlen == 0)
		return 0;
	*buf = XCALLOC(dlen, sizeof(uint8_t));

	for(size_t i = 0; i < slen; i++) {
		char c = string[i];
		int value = 0;
		if(c >= '0' && c <= '9') {
			value = (c - '0');
		} else if (c >= 'A' && c <= 'F') {
			value = (10 + (c - 'A'));
		} else if (c >= 'a' && c <= 'f') {
			 value = (10 + (c - 'a'));
		} else {
			debug_print(D_PROTO, "stopped at invalid char %u at pos %zu\n", c, i);
			return i/2;
		}
		(*buf)[(i/2)] |= value << (((i + 1) % 2) * 4);
	}
	return dlen;
}

char *hexdump(uint8_t *data, size_t len) {
	static const char hex[] = "0123456789abcdef";
	if(data == NULL) return strdup("<undef>");
	if(len == 0) return strdup("<none>");

	size_t rows = len / 16;
	if((len & 0xf) != 0) {
		rows++;
	}
	size_t rowlen = 16 * 2 + 16;		// 32 hex digits + 16 spaces per row
	rowlen += 16;				// ASCII characters per row
	rowlen += 10;				// extra space for separators
	size_t alloc_size = rows * rowlen + 1;	// terminating NULL
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

void unknown_proto_format_text(la_vstring * const vstr, void const * const data, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(data != NULL);
	ASSERT(indent >= 0);

	CAST_PTR(ostring, octet_string_t *, data);
// fmt_hexstring also checks this conditon, but when it hits, it prints "empty" or "none",
// which we want to avoid here
	if(ostring-> buf == NULL || ostring->len == 0) {
		return;
	}
	LA_ISPRINTF(vstr, indent, "Data (%zu bytes):\n", ostring->len);
	octet_string_format_text(vstr, ostring, indent+1);
	EOL(vstr);
}

la_type_descriptor const proto_DEF_unknown = {
	.format_text = unknown_proto_format_text,
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
