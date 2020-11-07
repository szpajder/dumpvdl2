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

#include <stdint.h>
#include <stdio.h>
#include "config.h"         // WITH_SQLITE
#include "dumpvdl2.h"       // NEW(), XFREE(), statsd_increment()
#include "ac_data.h"        // ac_data_entry

#ifdef WITH_SQLITE
#include <stdbool.h>
#include <string.h>         // strdup
#include <time.h>           // time_t, time()
#include <libacars/dict.h>  // la_dict
#include <libacars/hash.h>  // la_hash_*
#include <sqlite3.h>
#include "gs_data.h"        // uint_hash, uint_compare

static la_hash *ac_data_cache = NULL;
static time_t last_gc_time = 0L;
static size_t ac_cache_entry_count = 0;

typedef struct {
	time_t ctime;
	ac_data_entry *ac_data;
} ac_data_cache_entry;

#define AC_CACHE_TTL 1800L
#define AC_CACHE_GC_INTERVAL 305L

#define AC_CACHE_ENTRY_COUNT_ADD(x) do { \
	if((x) < 0 && ac_cache_entry_count < (unsigned long)(-(x))) { \
		ac_cache_entry_count = 0; \
	} else { \
		ac_cache_entry_count += (x); \
	} \
	statsd_set("ac_data.cache.entries", ac_cache_entry_count); \
} while(0)

static sqlite3 *db = NULL;
static sqlite3_stmt *stmt = NULL;

static void ac_data_entry_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	ac_data_entry *e = data;
	XFREE(e->registration);
	XFREE(e->icaotypecode);
	XFREE(e->operatorflagcode);
	XFREE(e->manufacturer);
	XFREE(e->type);
	XFREE(e->registeredowners);
	XFREE(e);
}

static void ac_data_cache_entry_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	ac_data_cache_entry *ce = data;
	ac_data_entry_destroy(ce->ac_data);
	XFREE(ce);
}

static void ac_data_cache_entry_create(uint32_t addr, ac_data_entry *ac_entry) {
	NEW(ac_data_cache_entry, ce);
	ce->ctime = time(NULL);
	ce->ac_data = ac_entry;
	NEW(uint32_t, key);
	*key = addr;
	la_hash_insert(ac_data_cache, key, ce);
	AC_CACHE_ENTRY_COUNT_ADD(1);
}

#define BS_DB_COLUMNS "Registration,ICAOTypeCode,OperatorFlagCode,Manufacturer,Type,RegisteredOwners"

static int ac_data_entry_from_db(uint32_t addr, ac_data_entry **result) {
	if(db == NULL || stmt == NULL) {
		return -1;
	}
	char hex_addr[7];
	if(snprintf(hex_addr, sizeof(hex_addr), "%06X", addr) != sizeof(hex_addr) - 1) {
		debug_print(D_CACHE, "could not convert addr %u to ICAO hex string - too large?\n", addr);
		return -2;
	}

	int rc = sqlite3_reset(stmt);
	if(rc != SQLITE_OK) {
		debug_print(D_CACHE, "sqlite3_reset() returned error %d\n", rc);
		statsd_increment("ac_data.db.errors");
		return rc;
	}
	rc = sqlite3_bind_text(stmt, 1, hex_addr, -1, SQLITE_STATIC);
	if(rc != SQLITE_OK) {
		debug_print(D_CACHE, "sqlite3_bind_text('%s') returned error %d\n", hex_addr, rc);
		statsd_increment("ac_data.db.errors");
		return rc;
	}
	rc = sqlite3_step(stmt);
	if(rc == SQLITE_ROW) {
		if(sqlite3_column_count(stmt) < 6) {
			debug_print(D_CACHE, "%s: not enough columns in the query result\n", hex_addr);
			return -3;
		}
		rc = SQLITE_OK;
		statsd_increment("ac_data.db.hits");
		if(result == NULL) {
			// The caller only wants the result code, not the data
			return rc;
		}
		// Create positive cache entry
		NEW(ac_data_entry, e);
		char const *field = NULL;
		if((field = (char *)sqlite3_column_text(stmt, 0)) != NULL) e->registration = strdup(field);
		if((field = (char *)sqlite3_column_text(stmt, 1)) != NULL) e->icaotypecode = strdup(field);
		if((field = (char *)sqlite3_column_text(stmt, 2)) != NULL) e->operatorflagcode = strdup(field);
		if((field = (char *)sqlite3_column_text(stmt, 3)) != NULL) e->manufacturer = strdup(field);
		if((field = (char *)sqlite3_column_text(stmt, 4)) != NULL) e->type = strdup(field);
		if((field = (char *)sqlite3_column_text(stmt, 5)) != NULL) e->registeredowners = strdup(field);
		ac_data_cache_entry_create(addr, e);
		*result = e;
	} else if(rc == SQLITE_DONE) {
		// Create negative cache entry
		ac_data_cache_entry_create(addr, NULL);
		// Empty result is not an error
		rc = SQLITE_OK;
		statsd_increment("ac_data.db.misses");
		if(result != NULL) {
			*result = NULL;
		}
	} else {
		debug_print(D_CACHE, "%s: unexpected query return code %d\n", hex_addr, rc);
		statsd_increment("ac_data.db.errors");
	}
	return rc;
}

bool is_cache_entry_expired(void const *key, void const *value, void *ctx) {
	UNUSED(key);
	ac_data_cache_entry const *cache_entry = value;
	time_t now = *(time_t *)ctx;
	return (cache_entry->ctime + AC_CACHE_TTL <= now);
}

ac_data_entry *ac_data_entry_lookup(uint32_t addr) {
	if(ac_data_cache == NULL) {
		return NULL;
	}

	// Periodic cache expiration
	time_t now = time(NULL);
	if(last_gc_time + AC_CACHE_GC_INTERVAL <= now) {
		int expired_cnt = la_hash_foreach_remove(ac_data_cache, is_cache_entry_expired, &now);
		debug_print(D_CACHE, "last_gc: %ld, now: %ld, expired %d cache entries\n", last_gc_time, now, expired_cnt);
		AC_CACHE_ENTRY_COUNT_ADD(-expired_cnt);
		last_gc_time = now;
	}

	ac_data_cache_entry *ce = la_hash_lookup(ac_data_cache, &addr);
	if(ce != NULL) {
		time_t now = time(NULL);
		if(is_cache_entry_expired(&addr, ce, &now)) {
			debug_print(D_CACHE, "%06X: expired cache entry (ctime %ld)\n", addr, ce->ctime);
			la_hash_remove(ac_data_cache, &addr);
			AC_CACHE_ENTRY_COUNT_ADD(-1);
		} else {
			statsd_increment("ac_data.cache.hits");
			debug_print(D_CACHE, "%06X: %s cache hit\n", addr, ce->ac_data ? "positive" : "negative");
			return ce->ac_data;
		}
	}
	// Cache entry missing or expired. Fetch it from DB.
	statsd_increment("ac_data.cache.misses");
	ac_data_entry *e = NULL;
	if(ac_data_entry_from_db(addr, &e) == SQLITE_OK) {
		debug_print(D_CACHE, "%06X: %sfound in BS DB\n", addr, e ? "" : "not ");
		return e;
	}
	debug_print(D_CACHE, "%06X: not found\n", addr);
	return NULL;
}

#ifdef WITH_STATSD
static char *ac_data_counters[] = {
	"ac_data.cache.hits",
	"ac_data.cache.misses",
	"ac_data.db.hits",
	"ac_data.db.misses",
	"ac_data.db.errors",
	NULL
};
#endif

int ac_data_init(char const *bs_db_file) {
	if(bs_db_file == NULL) {
		return -1;
	}
	db = NULL;

	int rc = sqlite3_open_v2(bs_db_file, &db, SQLITE_OPEN_READONLY, NULL);
	if(rc != 0){
		fprintf(stderr, "Can't open database %s: %s\n", bs_db_file, sqlite3_errmsg(db));
		goto fail;
	}
	rc = sqlite3_prepare_v2(db, "SELECT " BS_DB_COLUMNS " FROM Aircraft WHERE ModeS = ?", -1, &stmt, NULL);
	if(rc != SQLITE_OK) {
		fprintf(stderr, "%s: could not query Aircraft table: %s\n", bs_db_file, sqlite3_errmsg(db));
		goto fail;
	}
	ac_data_cache = la_hash_new(uint_hash, uint_compare, la_simple_free, ac_data_cache_entry_destroy);
	last_gc_time = time(NULL);
#ifdef WITH_STATSD
	statsd_initialize_counter_set(ac_data_counters);
#endif
	if(ac_data_entry_from_db(0, NULL) != SQLITE_OK) {
		fprintf(stderr, "%s: test query failed, database is unusable.\n", bs_db_file);
		goto fail;
	}
	fprintf(stderr, "%s: database opened\n", bs_db_file);
	return 0;
fail:
	la_hash_destroy(ac_data_cache);
	sqlite3_close(db);
	return -1;
}

void ac_data_destroy() {
	la_hash_destroy(ac_data_cache);
	sqlite3_finalize(stmt);
	sqlite3_close(db);
}

#else // !WITH_SQLITE

int ac_data_init(char const *bs_db_file) {
	UNUSED(bs_db_file);
	return -1;
}

ac_data_entry *ac_data_entry_lookup(uint32_t addr) {
	UNUSED(addr);
	return NULL;
}

void ac_data_destroy() { }

#endif // WITH_SQLITE
