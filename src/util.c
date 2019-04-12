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
#include "tlv.h"

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

char *fmt_hexstring(uint8_t *data, uint16_t len) {
	static const char hex[] = "0123456789abcdef";
	char *buf = NULL;
	if(data == NULL) return strdup("<undef>");
	if(len == 0) return strdup("none");
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

char *fmt_hexstring_with_ascii(uint8_t *data, uint16_t len) {
	if(data == NULL) return strdup("<undef>");
	if(len == 0) return strdup("none");
	char *buf = fmt_hexstring(data, len);
	int buflen = strlen(buf);
	buf = XREALLOC(buf, buflen + len + 4); // add tab, quotes, ascii dump and '\0'
	char *ptr = buf + buflen;
	*ptr++ = '\t';
	*ptr++ = '"';
	for(uint16_t i = 0; i < len; i++) {
		if(data[i] < 32 || data[i] > 126)	// replace non-printable chars
			*ptr++ = '.';
		else
			*ptr++ = data[i];
	}
	*ptr++ = '"';
	*ptr = '\0';
	return buf;
}

char *fmt_bitfield(uint8_t val, const dict *d) {
	if(val == 0) return strdup("none");
	char *buf = XCALLOC(256, sizeof(char));
	for(dict *ptr = (dict *)d; ptr->val != NULL; ptr++) {
		if((val & ptr->id) == ptr->id) {
			strcat(buf, (char *)ptr->val);
			strcat(buf, ", ");
		}
	}
	int slen = strlen(buf);
	if(slen == 0)
		strcat(buf, "none");
	else
		buf[slen-2] = '\0';	// throw out trailing delimiter
	return buf;
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
			debug_print("stopped at invalid char %u at pos %zu\n", c, i);
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

void append_hexstring_with_indent(la_vstring *vstr, uint8_t *data, size_t len, int indent) {
	ASSERT(vstr != NULL);
	ASSERT(indent >= 0);
	char *h = fmt_hexstring(data, len);
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
	append_hexstring_with_indent(vstr, ostring->buf, ostring->len, indent);
}

la_type_descriptor const proto_DEF_unknown = {
	.format_text = unknown_proto_format_text,
	.destroy = NULL
};

la_proto_node *unknown_proto_pdu_new(void *buf, size_t len) {
	octet_string_t *ostring = XCALLOC(1, sizeof(octet_string_t));
	ostring->buf = buf;
	ostring->len = len;
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_unknown;
	node->data = ostring;
	node->next = NULL;
	return node;
}
