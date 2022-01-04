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
#include "dumpvdl2.h"
#include "ap_data.h"        // ap_data_entry

#ifdef WITH_SQLITE
#include <stdbool.h>
#include <string.h>         // strdup
#include <time.h>           // time_t, time()
#include <libacars/hash.h>  // la_hash_*
#include <sqlite3.h>

static la_hash *ap_data_cache = NULL;
static time_t ap_last_gc_time = 0L;
static size_t ap_cache_entry_count = 0;

typedef struct {
	time_t ctime;
	ap_data_entry *ap_data;
} ap_data_cache_entry;

#define AP_CACHE_TTL 1800L
#define AP_CACHE_GC_INTERVAL 305L

#define AP_CACHE_ENTRY_COUNT_ADD(x) do { \
	if((x) < 0 && ap_cache_entry_count < (unsigned long)(-(x))) { \
		ap_cache_entry_count = 0; \
	} else { \
		ap_cache_entry_count += (x); \
	} \
	statsd_set("ap_data.cache.entries", ap_cache_entry_count); \
} while(0)

static sqlite3 *ap_db = NULL;
static sqlite3_stmt *ap_stmt = NULL;

static void ap_data_entry_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	ap_data_entry *e = data;
	XFREE(e->ap_name);
	XFREE(e->ap_city);
	XFREE(e->ap_country);
	XFREE(e->ap_icao_code);
	XFREE(e);
}

static void ap_data_cache_entry_destroy(void *data) {
	if(data == NULL) {
		return;
	}
	ap_data_cache_entry *ce = data;
	ap_data_entry_destroy(ce->ap_data);
	XFREE(ce);
}

static void ap_data_cache_entry_create(char const *ap_icao, ap_data_entry *ap_entry) {
	NEW(ap_data_cache_entry, ce);
	ce->ctime = time(NULL);
	ce->ap_data = ap_entry;
	NEW(char *, key);
	*key = (char *)ap_icao;
	la_hash_insert(ap_data_cache, key, ce);
	AP_CACHE_ENTRY_COUNT_ADD(1);
}

#define AP_DB_COLUMNS "NAME,CITY,COUNTRY,ICAO,LAT,LON"

static int ap_data_entry_from_db(char const *ap_icao, ap_data_entry **result) {
	if(ap_db == NULL || ap_stmt == NULL) {
		return -1;
	}

	int rc = sqlite3_reset(ap_stmt);
	if(rc != SQLITE_OK) {
		debug_print(D_CACHE, "sqlite3_reset() returned error %d\n", rc);
		statsd_increment("ap_data.db.errors");
		return rc;
	}
	rc = sqlite3_bind_text(ap_stmt, 1, (char *)ap_icao, -1, SQLITE_STATIC);
	if(rc != SQLITE_OK) {
		debug_print(D_CACHE, "sqlite3_bind_text('%s') returned error %d\n", ap_icao, rc);
		statsd_increment("ap_data.db.errors");
		return rc;
	}
	rc = sqlite3_step(ap_stmt);
	if(rc == SQLITE_ROW) {
		if(sqlite3_column_count(ap_stmt) < 6) {
			debug_print(D_CACHE, "%s: not enough columns in the query result\n", ap_icao);
			return -3;
		}
		rc = SQLITE_OK;
		statsd_increment("ap_data.db.hits");
		if(result == NULL) {
			// The caller only wants the result code, not the data
			return rc;
		}
		// Create positive cache entry
		NEW(ap_data_entry, e);
		char const *field = NULL;
		double dfield;
		if((field = (char *)sqlite3_column_text(ap_stmt, 0)) != NULL) e->ap_name = strdup(field);
		if((field = (char *)sqlite3_column_text(ap_stmt, 1)) != NULL) e->ap_city = strdup(field);
		if((field = (char *)sqlite3_column_text(ap_stmt, 2)) != NULL) e->ap_country = strdup(field);
		if((field = (char *)sqlite3_column_text(ap_stmt, 3)) != NULL) e->ap_icao_code = strdup(field);
		if((dfield = sqlite3_column_double(ap_stmt, 4))) e->ap_lat = dfield;
		if((dfield = sqlite3_column_double(ap_stmt, 5))) e->ap_lon = dfield;
		ap_data_cache_entry_create(ap_icao, e);
		*result = e;
	} else if(rc == SQLITE_DONE) {
		// Create negative cache entry
		ap_data_cache_entry_create(ap_icao, NULL);
		// Empty result is not an error
		rc = SQLITE_OK;
		statsd_increment("ap_data.db.misses");
		if(result != NULL) {
			*result = NULL;
		}
	} else {
		debug_print(D_CACHE, "%s: unexpected query return code %d\n", ap_icao, rc);
		statsd_increment("ap_data.db.errors");
	}
	return rc;
}

bool is_ap_cache_entry_expired(void const *key, void const *value, void *ctx) {
	UNUSED(key);
	ap_data_cache_entry const *cache_entry = value;
	time_t now = *(time_t *)ctx;
	return (cache_entry->ctime + AP_CACHE_TTL <= now);
}

ap_data_entry *ap_data_entry_lookup(char *ap_icao) {
	if(ap_data_cache == NULL) {
		return NULL;
	}

	// Periodic cache expiration
	time_t now = time(NULL);
	if(ap_last_gc_time + AP_CACHE_GC_INTERVAL <= now) {
		int expired_cnt = la_hash_foreach_remove(ap_data_cache, is_ap_cache_entry_expired, &now);
		debug_print(D_CACHE, "last_gc: %ld, now: %ld, expired %d cache entries\n", ap_last_gc_time, now, expired_cnt);
		AP_CACHE_ENTRY_COUNT_ADD(-expired_cnt);
		ap_last_gc_time = now;
	}

	ap_data_cache_entry *ce = la_hash_lookup(ap_data_cache, ap_icao);
	if(ce != NULL) {
		time_t now = time(NULL);
		if(is_ap_cache_entry_expired(ap_icao, ce, &now)) {
			debug_print(D_CACHE, "%s: expired cache entry (ctime %ld)\n", ap_icao, ce->ctime);
			la_hash_remove(ap_data_cache, ap_icao);
			AP_CACHE_ENTRY_COUNT_ADD(-1);
		} else {
			statsd_increment("ap_data.cache.hits");
			debug_print(D_CACHE, "%s: %s cache hit\n", ap_icao, ce->ap_data ? "positive" : "negative");
			return ce->ap_data;
		}
	}
	// Cache entry missing or expired. Fetch it from DB.
	statsd_increment("ap_data.cache.misses");
	ap_data_entry *e = NULL;
	if(ap_data_entry_from_db(ap_icao, &e) == SQLITE_OK) {
		debug_print(D_CACHE, "%s: %sfound in AP DB\n", ap_icao, e ? "" : "not ");
		return e;
	}
	debug_print(D_CACHE, "%s: not found\n", ap_icao);
	return NULL;
}

#ifdef WITH_STATSD
static char *ap_data_counters[] = {
	"ap_data.cache.hits",
	"ap_data.cache.misses",
	"ap_data.db.hits",
	"ap_data.db.misses",
	"ap_data.db.errors",
	NULL
};
#endif

bool local_la_hash_compare_keys_str(void const *key1, void const *key2) {
	return (key1 != NULL && key2 != NULL && strcmp(key1, key2) == 0);
}

int ap_data_init(char const *ap_db_file) {
	if(ap_db_file == NULL) {
		return -1;
	}
	ap_db = NULL;

	int rc = sqlite3_open_v2(ap_db_file, &ap_db, SQLITE_OPEN_READONLY, NULL);
	if(rc != 0){
		fprintf(stderr, "Can't open database %s: %s\n", ap_db_file, sqlite3_errmsg(ap_db));
		return -1;
	}
	rc = sqlite3_prepare_v2(ap_db, "SELECT " AP_DB_COLUMNS " FROM AIRPORTS WHERE ICAO = ?", -1, &ap_stmt, NULL);
	if(rc != SQLITE_OK) {
		fprintf(stderr, "%s: could not query Airports table: %s\n", ap_db_file, sqlite3_errmsg(ap_db));
		sqlite3_close(ap_db);
		return -1;
	}
	ap_data_cache = la_hash_new(la_hash_key_str, local_la_hash_compare_keys_str, la_simple_free, ap_data_cache_entry_destroy);
	ap_last_gc_time = time(NULL);
#ifdef WITH_STATSD
	statsd_initialize_counter_set(ap_data_counters);
#endif
	if(ap_data_entry_from_db(0, NULL) != SQLITE_OK) {
		fprintf(stderr, "%s: test query failed, airports database is unusable.\n", ap_db_file);
		goto fail;
	}
	fprintf(stderr, "%s: database opened\n", ap_db_file);
	return 0;
fail:
	sqlite3_close(ap_db);
	la_hash_destroy(ap_data_cache);
	return -1;
}

void ap_data_destroy() {
	la_hash_destroy(ap_data_cache);
	sqlite3_finalize(ap_stmt);
	sqlite3_close(ap_db);
}

#else // !WITH_SQLITE

int ap_data_init(char const *ap_db_file) {
	UNUSED(ap_db_file);
	return -1;
}

ap_data_entry *ap_data_entry_lookup(char *ap_icao) {
	UNUSED(ap_icao);
	return NULL;
}

void ap_data_destroy() { }

#endif // WITH_SQLITE
