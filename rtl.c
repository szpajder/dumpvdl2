#include <stdio.h>
#include <unistd.h>
#include <rtl-sdr.h>
#include "dumpvdl2.h"
#include "rtl.h"

static rtlsdr_dev_t *rtl = NULL;

void rtl_init(void *ctx, uint32_t device, int freq, int gain, int correction) {
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

	if(gain == RTL_AUTO_GAIN) {
		r = rtlsdr_set_tuner_gain_mode(rtl, 0);
		if (r < 0) {
			fprintf(stderr, "Failed to set automatic gain for device #%d: error %d\n", device, r);
			_exit(1);
		} else
			fprintf(stderr, "Device #%d: gain set to automatic\n", device);
	} else {
		r = rtlsdr_set_tuner_gain_mode(rtl, 1);
		r |= rtlsdr_set_tuner_gain(rtl, gain);
		if (r < 0) {
			fprintf(stderr, "Failed to set gain to %0.2f for device #%d: error %d\n",
				(float)gain / 10.0, device, r);
			_exit(1);
		} else
			fprintf(stderr, "Device #%d: gain set to %0.2f dB\n", device,
				(float)rtlsdr_get_tuner_gain(rtl) / 10.0);
	}

	r = rtlsdr_set_agc_mode(rtl, 0);
	if (r < 0) {
		fprintf(stderr, "Failed to disable AGC for device #%d: error %d\n", device, r);
		_exit(1);
	}
	rtlsdr_reset_buffer(rtl);
	fprintf(stderr, "Device %d started\n", device);
	if(rtlsdr_read_async(rtl, process_samples, ctx, RTL_BUFCNT, RTL_BUFSIZE) < 0) {
		fprintf(stderr, "Device #%d: async read failed\n", device);
		_exit(1);
	}
}

void rtl_cancel() {
	if(rtl != NULL)
		rtlsdr_cancel_async(rtl);
}
