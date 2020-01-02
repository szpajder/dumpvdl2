/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
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
#include <errno.h>		// errno
#include <stdio.h>		// fprintf, fopen, fscanf, fclose, perror
#include <string.h>		// strerror
#include <libacars/hash.h>	// la_hash_*
#include "dumpvdl2.h"		// debug_print
#include "gs_data.h"		// gs_data_entry

static la_hash *gs_data = NULL;

uint32_t uint_hash(void const *key) {
	return *(uint32_t *)key;
}

bool uint_compare(void const *key1, void const *key2) {
	return *(uint32_t *)key1 == *(uint32_t *)key2;
}

static void gs_data_entry_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	CAST_PTR(k, gs_data_entry *, data);
	XFREE(k->airport_code);
	XFREE(k->details);
	XFREE(k->location);
	XFREE(k);
}

int gs_data_import(char const *file) {
	if(file == NULL) {
		return -1;
	}
	int ret = -1;
	FILE *f = fopen(file, "r");
	if(f == NULL) {
		fprintf(stderr, "Could not open %s: %s\n", file, strerror(errno));
		goto fail;
	}
	uint32_t addr = 0;
	char airport_code[33];
	char details[257];
	char location[257];
	int result = 0;
	int cnt = 0;
	gs_data = la_hash_new(uint_hash, uint_compare, la_simple_free, gs_data_entry_destroy);
	while((result = fscanf(f, "%x [%256[^]]] [%256[^]]]\n", &addr, details, location)) != EOF) {
		cnt++;
		if(result != 3) {
			fprintf(stderr, "%s: parse error at line %d: expected 3 fields, got %d\n", file, cnt, result);
			ret = -1;
			goto fail;
		}
		if(sscanf(details, "%32s", airport_code) != 1) {
			fprintf(stderr, "%s: parse error at line %d: could not find airport code\n", file, cnt);
			ret = -1;
			goto fail;
		}
		debug_print("%d: addr: '%06X' apt_code: '%s' details: '%s' location: '%s'\n",
			cnt, addr, airport_code, details, location);
		NEW(uint32_t, key);
		NEW(gs_data_entry, entry);
		*key = addr;
		entry->airport_code = strdup(airport_code);
		entry->details = strdup(details);
		entry->location = strdup(location);
		la_hash_insert(gs_data, key, entry);
	}
	fprintf(stderr, "%s: read %d entries\n", file, cnt);
	ret = cnt;
	goto cleanup;

fail:
	la_hash_destroy(gs_data);
cleanup:
	fclose(f);
	return ret;
}

gs_data_entry *gs_data_entry_lookup(uint32_t addr) {
	if(gs_data == NULL) {
		return NULL;
	}
	return la_hash_lookup(gs_data, &addr);
}
