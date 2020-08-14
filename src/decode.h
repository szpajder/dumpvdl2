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

#ifndef _DECODE_H
#define _DECODE_H 1
#include <glib.h>               // GAsyncQueue
#include "output-common.h"      // vdl2_msg_metadata
#include "dumpvdl2.h"           // octet_string_t

void decode_vdl_frame(vdl2_channel_t *v);
void avlc_decoder_init();
void *avlc_decoder_thread(void *arg);
void avlc_decoder_thread_shutdown();
void avlc_decoder_queue_push(vdl2_msg_metadata *metadata, octet_string_t *frame, int flags);

#endif // !_DECODE_H
