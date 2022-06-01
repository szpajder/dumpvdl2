/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2022 Tomasz Lemiech <szpajder@gmail.com>
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
#ifndef REASSEMBLY_H
#define REASSEMBLY_H 1

#include <stdbool.h>
#include <sys/time.h>
#include <libacars/hash.h>
#include <libacars/reassembly.h>

typedef struct reasm_ctx_s reasm_ctx;
typedef struct reasm_table_s reasm_table;

typedef void *(reasm_get_key_func)(void const *msg);
typedef la_hash_func reasm_hash_func;
typedef la_hash_compare_func reasm_compare_func;
typedef la_hash_key_destroy_func reasm_key_destroy_func;
typedef struct {
	reasm_get_key_func *get_key;
	reasm_get_key_func *get_tmp_key;
	reasm_hash_func *hash_key;
	reasm_compare_func *compare_keys;
	reasm_key_destroy_func *destroy_key;
} reasm_table_funcs;

typedef struct {
	void const *pdu_info;           /* pointer to PDU metadata (eg. header),
	                                   used as hash key */

	uint8_t *fragment_data;         /* fragment data buffer */

	int fragment_data_len;          /* fragment data buffer length */

	int total_pdu_len;              /* Total length of the reassembled message */

	struct timeval rx_time;         /* fragment receive timestamp */

	struct timeval reasm_timeout;   /* reassembly timeout to be applied to this message */

	int offset;                      /* offset of the first octet of this fragment */

	bool is_final_fragment;         /* is this the final fragment of this message? */
} reasm_fragment_info;

typedef enum {
	REASM_UNKNOWN,
	REASM_COMPLETE,
	REASM_IN_PROGRESS,
	REASM_SKIPPED,
	REASM_DUPLICATE,
	REASM_BAD_OFFSET,
	REASM_BAD_LEN,
	REASM_OVERLAP,
	REASM_ARGS_INVALID
} reasm_status;
#define REASM_STATUS_MAX REASM_ARGS_INVALID

// reassembly.c
reasm_status reasm_fragment_add(la_reasm_table *rtable, reasm_fragment_info const *finfo);
int reasm_payload_get(la_reasm_table *rtable, void const *msg_info, uint8_t **result);
char const *reasm_status_name_get(reasm_status status);

#endif // !REASSEMBLY_H
