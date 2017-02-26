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
#include <rtl-sdr.h>
#include "dumpvdl2.h"
#include "rtl.h"

static rtlsdr_dev_t *rtl = NULL;

/* taken from librtlsdr-keenerd, (c) Kyle Keen */
static int nearest_gain(rtlsdr_dev_t *dev, int target_gain) {
	int i, r, err1, err2, count, nearest;
	int* gains;
	r = rtlsdr_set_tuner_gain_mode(dev, 1);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to enable manual gain.\n");
		return r;
	}
	count = rtlsdr_get_tuner_gains(dev, NULL);
	if (count <= 0) {
		return -1;
	}
	gains = XCALLOC(count, sizeof(int));
	count = rtlsdr_get_tuner_gains(dev, gains);
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

void rtl_init(vdl2_state_t *ctx, uint32_t device, int freq, float gain, int correction) {
	int r;

	r = rtlsdr_open(&rtl, device);
	if(rtl == NULL) {
		fprintf(stderr, "Failed to open rtlsdr device #%u: error %d\n", device, r);
		_exit(1);
	}
	r = rtlsdr_set_sample_rate(rtl, RTL_RATE);
	if (r < 0) {
		fprintf(stderr, "Failed to set sample rate for device #%d: error %d\n", device, r);
		_exit(1);
	}
	r = rtlsdr_set_center_freq(rtl, freq);
	if (r < 0) {
		fprintf(stderr, "Failed to set frequency for device #%d: error %d\n", device, r);
		_exit(1);
	}
	fprintf(stderr, "Center frequency set to %u Hz\n", freq);
	r = rtlsdr_set_freq_correction(rtl, correction);
	if (r < 0 && r != -2 ) {
		fprintf(stderr, "Failed to set freq correction for device #%d error %d\n", device, r);
		_exit(1);
	}

	if(gain == SDR_AUTO_GAIN) {
		r = rtlsdr_set_tuner_gain_mode(rtl, 0);
		if (r < 0) {
			fprintf(stderr, "Failed to set automatic gain for device #%d: error %d\n", device, r);
			_exit(1);
		} else
			fprintf(stderr, "Device #%d: gain set to automatic\n", device);
	} else {
		int ngain = nearest_gain(rtl, (int)(gain * 10.f));
		if(ngain < 0) {
			fprintf(stderr, "Failed to read supported gain list for device #%d: error %d\n", device, ngain);
			_exit(1);
		}
		r = rtlsdr_set_tuner_gain_mode(rtl, 1);
		r |= rtlsdr_set_tuner_gain(rtl, ngain);
		if (r < 0) {
			fprintf(stderr, "Failed to set gain to %0.2f for device #%d: error %d\n",
				(float)ngain / 10.f, device, r);
			_exit(1);
		} else
			fprintf(stderr, "Device #%d: gain set to %0.2f dB\n", device,
				(float)rtlsdr_get_tuner_gain(rtl) / 10.f);
	}

	r = rtlsdr_set_agc_mode(rtl, 0);
	if (r < 0) {
		fprintf(stderr, "Failed to disable AGC for device #%d: error %d\n", device, r);
		_exit(1);
	}
	rtlsdr_reset_buffer(rtl);
	fprintf(stderr, "Device %d started\n", device);
	ctx->sbuf = XCALLOC(RTL_BUFSIZE / sizeof(uint8_t), sizeof(float));
	process_buf_uchar_init();
	if(rtlsdr_read_async(rtl, process_buf_uchar, ctx, RTL_BUFCNT, RTL_BUFSIZE) < 0) {
		fprintf(stderr, "Device #%d: async read failed\n", device);
		_exit(1);
	}
}

void rtl_cancel() {
	if(rtl != NULL)
		rtlsdr_cancel_async(rtl);
}
