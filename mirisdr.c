/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
 *  Copyright (c) 2017 Tomasz Lemiech <szpajder@gmail.com>
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
#include <stdlib.h>
#include <unistd.h>
#include <mirisdr.h>
#include "dumpvdl2.h"
#include "mirisdr.h"

static mirisdr_dev_t *mirisdr = NULL;

/* taken from librtlsdr-keenerd, (c) Kyle Keen */
static int mirisdr_nearest_gain(mirisdr_dev_t *dev, int target_gain) {
	int i, r, err1, err2, count, nearest;
	int* gains;
	r = mirisdr_set_tuner_gain_mode(dev, 1);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to enable manual gain.\n");
		return r;
	}
	count = mirisdr_get_tuner_gains(dev, NULL);
	if (count <= 0) {
		return -1;
	}
	gains = XCALLOC(count, sizeof(int));
	count = mirisdr_get_tuner_gains(dev, gains);
	nearest = gains[0];
	for (i = 0; i < count; i++) {
		err1 = abs(target_gain - nearest);
		err2 = abs(target_gain - gains[i]);
		if (err2 < err1) {
			nearest = gains[i];
		}
	}
	free(gains);
	return nearest;
}

void mirisdr_init(vdl2_state_t *ctx, uint32_t device, int flavour, uint32_t freq, float gain, int freq_offset, int usb_xfer_mode) {
	int r;

	mirisdr_hw_flavour_t hw_flavour;
	switch(flavour) {
	case 0:
		hw_flavour = MIRISDR_HW_DEFAULT;
		break;
	case 1:
		hw_flavour = MIRISDR_HW_SDRPLAY;
		break;
	default:
		fprintf(stderr, "Unknown device variant %u\n", flavour);
		_exit(1);
	}
	r = mirisdr_open(&mirisdr, hw_flavour, device);
	if(mirisdr == NULL) {
		fprintf(stderr, "Failed to open mirisdr device #%u: error %d\n", device, r);
		_exit(1);
	}

	if(usb_xfer_mode == 0)
		r = mirisdr_set_transfer(mirisdr, "ISOC");
	else if(usb_xfer_mode == 1)
		r = mirisdr_set_transfer(mirisdr, "BULK");
	else {
		fprintf(stderr, "Invalid USB transfer mode\n");
		_exit(1);
	}
	if (r < 0) {
		fprintf(stderr, "Failed to set transfer mode for device #%d: error %d\n", device, r);
		_exit(1);
	}
	fprintf(stderr, "Using USB transfer mode %s\n", mirisdr_get_transfer(mirisdr));

	r = mirisdr_set_sample_rate(mirisdr, MIRISDR_RATE);
	if (r < 0) {
		fprintf(stderr, "Failed to set sample rate for device #%d: error %d\n", device, r);
		_exit(1);
	}
	r = mirisdr_set_center_freq(mirisdr, freq - freq_offset);
	if (r < 0) {
		fprintf(stderr, "Failed to set frequency for device #%d: error %d\n", device, r);
		_exit(1);
	}
	fprintf(stderr, "Center frequency set to %u Hz\n", freq - freq_offset);

	if(gain == SDR_AUTO_GAIN) {
		r = mirisdr_set_tuner_gain_mode(mirisdr, 0);
		if (r < 0) {
			fprintf(stderr, "Failed to set automatic gain for device #%d: error %d\n", device, r);
			_exit(1);
		} else
			fprintf(stderr, "Device #%d: gain set to automatic\n", device);
	} else {
		int ngain = mirisdr_nearest_gain(mirisdr, (int)gain);
		if(ngain < 0) {
			fprintf(stderr, "Failed to read supported gain list for device #%d: error %d\n", device, ngain);
			_exit(1);
		}
		r = mirisdr_set_tuner_gain_mode(mirisdr, 1);
		r |= mirisdr_set_tuner_gain(mirisdr, ngain);
		if (r < 0) {
			fprintf(stderr, "Failed to set gain to %d for device #%d: error %d\n",
				ngain, device, r);
			_exit(1);
		} else
			fprintf(stderr, "Device #%d: gain set to %d dB\n", device,
				mirisdr_get_tuner_gain(mirisdr));
	}

	r = mirisdr_set_sample_format(mirisdr, "252_S16");
	if (r < 0) {
		fprintf(stderr, "Failed to set sample format for device #%d: error %d\n", device, r);
		_exit(1);
	}
	mirisdr_reset_buffer(mirisdr);
	fprintf(stderr, "Device %d started\n", device);
	ctx->sbuf = XCALLOC(MIRISDR_BUFSIZE / sizeof(int16_t), sizeof(float));
	if(mirisdr_read_async(mirisdr, process_buf_short, ctx, MIRISDR_BUFCNT, MIRISDR_BUFSIZE) < 0) {
		fprintf(stderr, "Device #%d: async read failed\n", device);
		_exit(1);
	}
}

void mirisdr_cancel() {
	if(mirisdr != NULL)
		mirisdr_cancel_async(mirisdr);
}
