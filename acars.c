/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2018 Tomasz Lemiech <szpajder@gmail.com>
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
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libacars/libacars.h>		// la_proto_node, la_proto_tree_destroy, la_proto_tree_format_text
#include <libacars/acars.h>		// la_acars_parse
#include <libacars/vstring.h>		// la_vstring, la_vstring_append_sprintf
#include "dumpvdl2.h"
#include "acars.h"

la_proto_node *parse_acars(uint8_t *buf, uint32_t len, uint32_t *msg_type) {
	la_msg_dir msg_dir = LA_MSG_DIR_UNKNOWN;
	if(*msg_type & MSGFLT_SRC_AIR) {
		msg_dir = LA_MSG_DIR_AIR2GND;
	} else if(*msg_type & MSGFLT_SRC_GND) {
		msg_dir = LA_MSG_DIR_GND2AIR;
	}
	if(msg_dir == LA_MSG_DIR_UNKNOWN) {
		debug_print("%s", "Message direction is unknown!\n");
		return NULL;
	}
	return la_acars_parse(buf, len, msg_dir);
}

static void output_acars_pp(la_proto_node const * const node) {
	if(node == NULL || node->td != &la_DEF_acars_message) {
		return;
	}
	la_acars_msg *msg = node->data;
	char *txt = strdup(msg->txt);
	for(char *ptr = txt; *ptr != 0; ptr++) {
		if (*ptr == '\n' || *ptr == '\r') {
			*ptr = ' ';
		}
	}
	la_vstring *vstr = la_vstring_new();
	la_vstring_append_sprintf(vstr, "AC%1c %7s %1c %2s %1c %4s %6s %s",
		msg->mode, msg->reg, msg->ack, msg->label, msg->block_id, msg->no, msg->flight_id, txt);

	if(write(pp_sockfd, vstr->str, vstr->len) < 0) {
		debug_print("write(pp_sockfd) error: %s", strerror(errno));
	}
	XFREE(txt);
	la_vstring_destroy(vstr, true);
}

void output_acars(void const *msg) {
	if(msg == NULL) {
		return;
	}
	la_proto_node *node = (la_proto_node *)msg;
	la_vstring *vstr = la_proto_tree_format_text(NULL, node);
	fprintf(outf, "%s", vstr->str);
	la_vstring_destroy(vstr, true);
	if(pp_sockfd > 0) {
		output_acars_pp(node);
	}
}

void destroy_acars(void *msg) {
	la_proto_tree_destroy((la_proto_node *)msg);
}
