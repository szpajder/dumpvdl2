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

#include <stddef.h>         // ptrdiff_t
#include <string.h>         // strsep
#include <libacars/hash.h>  // la_hash
#include "dumpvdl2.h"       // NEW, XFREE, debug_print
#include "kvargs.h"

struct kvargs_s {
	la_hash *h;
};

#define KV_ERR_NO_ERROR     0
#define KV_ERR_NO_INPUT     1
#define KV_ERR_NO_KEY       2
#define KV_ERR_NO_VALUE     3

char const *kvargs_get_errstr(int err) {
	static dict const kvargs_error_strings[] = {
		{ .id = KV_ERR_NO_ERROR,  .val = "success" },
		{ .id = KV_ERR_NO_INPUT,  .val = "no key-value string given" },
		{ .id = KV_ERR_NO_KEY,    .val = "no key name given" },
		{ .id = KV_ERR_NO_VALUE,  .val = "no value given" },
		{ .id = 0,                .val = NULL }
	};
	char *ret = (char *)dict_search(kvargs_error_strings, err);
	return (ret != NULL ? ret : "unknown error");
}

kvargs *kvargs_new() {
	NEW(kvargs, kv);
	kv->h = la_hash_new(NULL, NULL, la_simple_free, la_simple_free);
	return kv;
}
	
kvargs_parse_result kvargs_from_string(char *string) {
	kvargs *kv = NULL;
	int err = 0;
	ptrdiff_t err_pos = 0;

	if(string == NULL) {
		err = KV_ERR_NO_INPUT;
		goto fail;
	}

	kv = kvargs_new();
	char *start = string, *kvpair = NULL, *key = NULL;

	do {
		kvpair = strsep(&string, ",");
		key = strsep(&kvpair, "=");
		if(key[0] == '\0') {
			err_pos = key - start;
			err = KV_ERR_NO_KEY;
			goto fail;
		}
		// kvpair points at the value now
		if(kvpair == NULL) {
			err_pos = key + strlen(key) - start;
			err = KV_ERR_NO_VALUE;
			goto fail;
		}
		debug_print(D_MISC, "key: '%s' val: '%s'\n", key, kvpair);
		la_hash_insert(kv->h, strdup(key), strdup(kvpair));
	} while(string != NULL);

	goto end;

fail:
	debug_print(D_MISC, "kvpair error %d at position %td\n", err, err_pos);
	kvargs_destroy(kv);
end:
	return (kvargs_parse_result){
		.result = kv,
		.err_pos = err_pos,
		.err = err
	};
}

char *kvargs_get(kvargs const *kv, char const *key) {
	return (char *)la_hash_lookup(kv->h, key);
}

void kvargs_destroy(kvargs *kv) {
	if(kv != NULL) {
		la_hash_destroy(kv->h);
		XFREE(kv);
	}
}
