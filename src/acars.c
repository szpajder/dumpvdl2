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
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>               // struct timeval
#include <libacars/libacars.h>      // la_proto_node, la_proto_tree_destroy, la_proto_tree_format_text
#include <libacars/acars.h>         // la_acars_parse, la_proto_tree_find_acars
#include <libacars/adsc.h>          // la_proto_tree_find_adsc
#include <libacars/cpdlc.h>         // la_proto_tree_find_cpdlc
#include <libacars/vstring.h>       // la_vstring, la_vstring_append_sprintf
#include <libacars/reassembly.h>    // la_reasm_ctx
#include "dumpvdl2.h"
#include "acars.h"

static void update_msg_type(uint32_t *msg_type, la_proto_node *root) {
	la_proto_node *node = la_proto_tree_find_acars(root);
	if(node == NULL) {
		debug_print(D_PROTO, "proto tree contains no ACARS message\n");
		return;
	}
	CAST_PTR(amsg, la_acars_msg *, node->data);
	if(amsg->err == true) {
		debug_print(D_PROTO, "amsg->err is true, skipping\n");
		return;
	}
	if(strlen(amsg->txt) > 0) {
		debug_print(D_PROTO, "MSGFLT_ACARS_DATA\n");
		*msg_type |= MSGFLT_ACARS_DATA;
	} else {
		debug_print(D_PROTO, "MSGFLT_ACARS_NODATA\n");
		*msg_type |= MSGFLT_ACARS_NODATA;
	}

	la_proto_node *node2 = la_proto_tree_find_cpdlc(node);
	if(node2 != NULL) {
		debug_print(D_PROTO, "MSGFLT_CPDLC\n");
		*msg_type |= MSGFLT_CPDLC;
	}

	node2 = la_proto_tree_find_adsc(node);
	if(node2 != NULL) {
		debug_print(D_PROTO, "MSGFLT_ADSC\n");
		*msg_type |= MSGFLT_ADSC;
	}
}

#ifdef WITH_STATSD
static void update_statsd_acars_metrics(la_msg_dir msg_dir, la_proto_node *root) {
	static dict const reasm_status_counter_names[] = {
		{ .id = LA_REASM_UNKNOWN, .val = "acars.reasm.unknown" },
		{ .id = LA_REASM_COMPLETE, .val = "acars.reasm.complete" },
		// { .id = LA_REASM_IN_PROGRESS, .val = "acars.reasm.in_progress" },    // report final states only
		{ .id = LA_REASM_SKIPPED, .val = "acars.reasm.skipped" },
		{ .id = LA_REASM_DUPLICATE, .val = "acars.reasm.duplicate" },
		{ .id = LA_REASM_FRAG_OUT_OF_SEQUENCE, .val = "acars.reasm.out_of_seq" },
		{ .id = LA_REASM_ARGS_INVALID, .val = "acars.reasm.invalid_args" },
		{ .id = 0, .val = NULL }
	};
	la_proto_node *node = la_proto_tree_find_acars(root);
	if(node == NULL) {
		return;
	}
	CAST_PTR(amsg, la_acars_msg *, node->data);
	if(amsg->err == true) {
		return;
	}
	CAST_PTR(metric, char *, dict_search(reasm_status_counter_names, amsg->reasm_status));
	if(metric == NULL) {
		return;
	}
	statsd_increment_per_msgdir(msg_dir, metric);
}
#endif

la_proto_node *parse_acars(uint8_t *buf, uint32_t len, uint32_t *msg_type,
		la_reasm_ctx *reasm_ctx, struct timeval rx_time) {
	la_msg_dir msg_dir = LA_MSG_DIR_UNKNOWN;
	if(*msg_type & MSGFLT_SRC_AIR) {
		msg_dir = LA_MSG_DIR_AIR2GND;
	} else if(*msg_type & MSGFLT_SRC_GND) {
		msg_dir = LA_MSG_DIR_GND2AIR;
	}
	la_proto_node *node = la_acars_parse_and_reassemble(buf, len, msg_dir, reasm_ctx, rx_time);
	update_msg_type(msg_type, node);
#ifdef WITH_STATSD
	update_statsd_acars_metrics(msg_dir, node);
#endif
	return node;
}

la_vstring *acars_format_pp(la_proto_node *tree) {
	la_proto_node *acars_node = la_proto_tree_find_acars(tree);
	if(acars_node == NULL) {
		return NULL;
	}
	la_acars_msg *msg = acars_node->data;
	if(msg->err == true) {
		return NULL;
	}
	char *txt = strdup(msg->txt);
	for(char *ptr = txt; *ptr != 0; ptr++) {
		if (*ptr == '\n' || *ptr == '\r') {
			*ptr = ' ';
		}
	}
	la_vstring *vstr = la_vstring_new();
	la_vstring_append_sprintf(vstr, "AC%1c %7s %1c %2s %1c %3s%c %6s %s",
			msg->mode, msg->reg, msg->ack, msg->label, msg->block_id,
			msg->msg_num, msg->msg_num_seq, msg->flight_id, txt);

	XFREE(txt);
	return vstr;
}

la_vstring *acars_format_json(la_proto_node *tree) {
	la_proto_node *acars_node = la_proto_tree_find_acars(tree);
	if(acars_node == NULL) {
		return NULL;
	}
	la_acars_msg *msg = acars_node->data;
	if(msg->err == true) {
		return NULL;
	}

	char *txt = strdup(msg->txt);
	int newlen = 1;

	for (char* p = txt; *p; p++)
  	newlen += (*p == '\n' || *p == '\r') ? 2 : 1;

	char *newtxt = malloc(newlen);

	for (char *p = str, *q = newtxt;; p++) {
    if (*p == '\n')
    {
			*q++ = '\\';
			*q++ = 'n';
    }
		else if (*p == '\r')
		{
			*q++ = '\\';
			*q++ = 'r';
		}
    else if (!(*q++ = *p))
			break;
	}

	la_vstring *vstr = la_vstring_new();
	la_vstring_append_sprintf(vstr,
			"{ \"mode\": %1c, \"ident\": \"%7s\", \"ack\": \"%1c\", \"label\": \"%2s\", \"block_id\": \"%1c\", \"message_number\": \"%3s%c\", \"flight\": \"%6s\", \"text\": \"%s\" }",
			msg->mode, msg->reg, msg->ack, msg->label, msg->block_id,
			msg->msg_num, msg->msg_num_seq, msg->flight_id, newtxt);

	XFREE(txt);
	XFREE(newtxt);
	return vstr;
}
