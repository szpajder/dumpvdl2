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
#include "x25.h"

// CLNP header dissection is not implemented yet
#define CLNP_MIN_LEN 2
#define CLNP_COMPRESSED_INIT_MIN_LEN 4

typedef struct {
	uint8_t proto;
	uint8_t data_valid;
	void *data;
	uint32_t datalen;
} clnp_pdu_t;

// clnp.c
clnp_pdu_t *parse_clnp_pdu(uint8_t *buf, uint32_t len, uint32_t *msg_type);
clnp_pdu_t *parse_clnp_compressed_init_pdu(uint8_t *buf, uint32_t len, uint32_t *msg_type);
void output_clnp(clnp_pdu_t *pdu);
void output_clnp_compressed(clnp_pdu_t *pdu);
