/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
 *  Copyright (c) 2017 Fabrice Crohas <fcrohas@gmail.com>
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
#include <signal.h>
#include "dumpvdl2.h"
#define ACC_SHIFT		14		// sets time constant of averaging filter
#define MIN_GAIN_THRESH		6		// increase gain if peaks below this
#define MAX_GAIN_THRESH		9		// decrease gain if peaks above this
#define MAX_RSP_GAIN		59
#define MIN_RSP_GAIN		20		// Not extended range so limit is 20
#define MAX_LNA_STATE		8		// 8 according to RSP2 documentation for frf < 420 Mhz
#define ASYNC_BUF_NUMBER	15
#define ASYNC_BUF_SIZE	 	(32*16384)	// 512k shorts
#define MODES_AUTO_GAIN		-100		// Use automatic gain
#define SDRPLAY_OVERSAMPLE	20
#define SDRPLAY_RATE (SYMBOL_RATE * SPS * SDRPLAY_OVERSAMPLE)

// exit flag sighandler
extern int do_exit;

// sdrplay struct
struct sdrplay_t {
	int autogain;
	int sdrplaySamplesPerPacket;
	unsigned char *sdrplay_data;
	int lna_state;
	int gRdB;
	int stop;
	int max_sig;
	int max_sig_acc;
	int data_index;
	void *context;
};

// sdrplay basic methods
void sdrplay_init(vdl2_state_t *ctx, char *dev, char *antenna, uint32_t freq, float gain, int ppm_error,
	int enable_biast, int enable_notch_filter, int enable_agc);
void sdrplay_cancel();
void sdrplay_streamCallback(short *xi, short *xq, unsigned int firstSampleNum, int grChanged, int rfChanged,
	int fsChanged, unsigned int numSamples, unsigned int reset, void *cbContext);
void sdrplay_gainCallback(unsigned int gRdB, unsigned int lnaGRdB, void *cbContext);
