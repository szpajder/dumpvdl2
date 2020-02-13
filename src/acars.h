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
#include <sys/time.h>               // struct timeval
#include <libacars/libacars.h>      // la_proto_node
#include <libacars/reassembly.h>    // la_reasm_ctx

// acars.c
la_proto_node *parse_acars(uint8_t *buf, uint32_t len, uint32_t *msg_type,
		la_reasm_ctx *reasm_ctx, struct timeval rx_time);
void acars_output_pp(la_proto_node *tree);
