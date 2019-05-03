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
#include <stdint.h>
#include <stdbool.h>
#include <libacars/libacars.h>		// la_proto_node
#include "config.h"
#include "x25.h"

// CLNP header dissection is not implemented yet
#define CLNP_MIN_LEN 2
#define CLNP_COMPRESSED_INIT_MIN_LEN 4

typedef struct {
	bool err;
} clnp_pdu_t;

typedef struct {
#ifdef IS_BIG_ENDIAN
	uint8_t type:4;
	uint8_t priority:4;
#else
	uint8_t priority:4;
	uint8_t type:4;
#endif
	uint8_t lifetime;
	union {
		uint8_t val;
		struct {
#ifdef IS_BIG_ENDIAN
			uint8_t p:1;
			uint8_t q:1;
			uint8_t r:1;
			uint8_t st:1;
			uint8_t ce:1;
			uint8_t tc:1;
			uint8_t et:1;
			uint8_t ec:1;
#else
			uint8_t ec:1;
			uint8_t et:1;
			uint8_t tc:1;
			uint8_t ce:1;
			uint8_t st:1;
			uint8_t r:1;
			uint8_t q:1;
			uint8_t p:1;
#endif
		} fields;
	} flags;
#ifdef IS_BIG_ENDIAN
	uint8_t exp:1;
	uint8_t lref_a:7;
#else
	uint8_t lref_a:7;
	uint8_t exp:1;
#endif
} clnp_compressed_init_data_pdu_hdr_t;

typedef struct {
	clnp_compressed_init_data_pdu_hdr_t *hdr;
	uint16_t lref;
	uint16_t pdu_id;
	bool pdu_id_present;
	bool err;
} clnp_compressed_init_data_pdu_t;

// clnp.c
la_proto_node *clnp_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type);
la_proto_node *clnp_compressed_init_data_pdu_parse(uint8_t *buf, uint32_t len, uint32_t *msg_type);
