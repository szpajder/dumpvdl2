/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2026 Tomasz Lemiech <szpajder@gmail.com>
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
#include <inttypes.h>           // PRIx64, PRIu64
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>             // strtol()
#include <string.h>             // strlen()
#include <strings.h>            // strcasecmp()
#include <unistd.h>             // _exit(), usleep()
#include <airspyhf.h>
#include "dumpvdl2.h"           // do_exit, process_buf_cf32(), XCALLOC(), XFREE(), ASSERT()
#include "airspyhf.h"

// NOTE: libairspyhf owns the airspyhf_* namespace (airspyhf_start() in
// particular), hence the input_ prefix on the entry points defined here.

static struct airspyhf_device *airspyhf = NULL;

// libairspyhf has no equivalent of airspy_error_name(), so spell the few
// result codes out here.
static char const *airspyhf_strerror(int e) {
	switch(e) {
		case AIRSPYHF_SUCCESS:
			return "success";
		case AIRSPYHF_ERROR:
			return "error";
		case AIRSPYHF_UNSUPPORTED:
			return "unsupported (device firmware too old?)";
		default:
			return "unknown error";
	}
}

static void airspyhf_verbose_device_search(uint64_t *serials, int count) {
	fprintf(stderr, "Found %d device(s):\n", count);
	for(int i = 0; i < count; i++) {
		fprintf(stderr, "  %d:  SN: %016" PRIx64 "\n", i, serials[i]);
	}
	fprintf(stderr, "\n");
}

// Opens the Airspy HF+ device selected by dev (a device index or a serial
// number, possibly abbreviated) and decides which sample rate to run it at.
// Returns the chosen rate, which the caller needs in order to set up the
// resampler before the demodulator threads are started.
uint32_t input_airspyhf_open_device(char *dev, uint32_t sample_rate) {
	uint64_t serials[AIRSPYHF_MAX_DEVICES];
	int count = airspyhf_list_devices(serials, AIRSPYHF_MAX_DEVICES);
	if(count < 1) {
		fprintf(stderr, "No supported devices found.\n");
		_exit(1);
	}
	if(count > AIRSPYHF_MAX_DEVICES) {
		count = AIRSPYHF_MAX_DEVICES;
	}
	airspyhf_verbose_device_search(serials, count);

	// Select the device by index, then by exact serial number, then by a
	// suffix of it (which is what the device labels usually show).
	uint64_t serial = 0;
	char *endptr = NULL;
	long index = strtol(dev, &endptr, 10);
	if(endptr[0] == '\0' && index >= 0 && index < count && strlen(dev) < 5) {
		serial = serials[index];
	} else {
		size_t devlen = strlen(dev);
		for(int i = 0; i < count && serial == 0; i++) {
			char sn[17];
			snprintf(sn, sizeof(sn), "%016" PRIx64, serials[i]);
			if(strcasecmp(dev, sn) == 0 ||
					(devlen < 16 && strcasecmp(dev, sn + 16 - devlen) == 0)) {
				serial = serials[i];
			}
		}
	}
	if(serial == 0) {
		fprintf(stderr, "No matching devices found.\n");
		_exit(1);
	}
	int r = airspyhf_open_sn(&airspyhf, serial);
	if(r != AIRSPYHF_SUCCESS) {
		fprintf(stderr, "Failed to open Airspy HF+ device %016" PRIx64 ": %s\n",
				serial, airspyhf_strerror(r));
		_exit(1);
	}
	fprintf(stderr, "Using device with SN: %016" PRIx64 "\n", serial);

	// Read the list of rates the device supports. No Airspy HF+ rate is a
	// multiple of SYMBOL_RATE * SPS, so whichever one gets picked, the sample
	// stream is going to be converted (see --resampler).
	uint32_t rate_count = 0;
	r = airspyhf_get_samplerates(airspyhf, &rate_count, 0);
	if(r != AIRSPYHF_SUCCESS || rate_count < 1) {
		fprintf(stderr, "Failed to read supported sample rates: %s\n", airspyhf_strerror(r));
		_exit(1);
	}
	uint32_t *rates = XCALLOC(rate_count, sizeof(uint32_t));
	r = airspyhf_get_samplerates(airspyhf, rates, rate_count);
	if(r != AIRSPYHF_SUCCESS) {
		fprintf(stderr, "Failed to read supported sample rates: %s\n", airspyhf_strerror(r));
		_exit(1);
	}
	uint32_t chosen = 0;
	if(sample_rate == 0) {
		// Default to the *highest* supported rate, which is the opposite of
		// what the Airspy R2/Mini driver does. Those start at a few Msps, so
		// the lowest rate is both the cheapest to process and wide enough for
		// the whole VDL2 band. The HF+ tops out at 768 kHz (912 kHz on some
		// firmware) and goes down to 192 kHz, which would not fit the band -
		// and even the highest rate is cheap to process by comparison.
		for(uint32_t i = 0; i < rate_count; i++) {
			if(rates[i] > chosen) {
				chosen = rates[i];
			}
		}
	} else {
		for(uint32_t i = 0; i < rate_count; i++) {
			if(rates[i] == sample_rate) {
				chosen = sample_rate;
				break;
			}
		}
		if(chosen == 0) {
			fprintf(stderr, "Sample rate %u is not supported by this device. Supported rates:",
					sample_rate);
			for(uint32_t i = 0; i < rate_count; i++) {
				fprintf(stderr, " %u", rates[i]);
			}
			fprintf(stderr, "\n");
			_exit(1);
		}
	}
	XFREE(rates);
	// Must happen before the frequency is set - the rate decides whether the
	// device tunes zero-IF or low-IF.
	r = airspyhf_set_samplerate(airspyhf, chosen);
	if(r != AIRSPYHF_SUCCESS) {
		fprintf(stderr, "Failed to set sample rate to %u: %s\n", chosen, airspyhf_strerror(r));
		_exit(1);
	}
	return chosen;
}

static int airspyhf_rx_callback(airspyhf_transfer_t *transfer) {
	if(do_exit) {
		return -1;
	}
	// dropped_samples counts the samples lost since the previous callback,
	// not a running total - the library resets its drop counter every time
	// it queues a buffer.
	if(transfer->dropped_samples > 0) {
		fprintf(stderr, "Warning: dropped %" PRIu64 " samples "
				"(sample rate too high for this machine?)\n",
				transfer->dropped_samples);
	}
	// libairspyhf delivers airspyhf_complex_float_t, ie. pairs of floats
	// already scaled to <-1;1>, which is what process_buf_cf32() expects.
	// Block sizes are decided by the library and may vary, so sbuf is not
	// preallocated here - process_buf_cf32() sizes it at the only point where
	// doing so is safe.
	process_buf_cf32((float *)transfer->samples, (uint32_t)transfer->sample_count * 2, NULL);
	return 0;
}

void input_airspyhf_start(vdl2_state_t *ctx, uint32_t centerfreq, int agc, int agc_threshold,
		int att, int lna, int correction, int biast) {
	UNUSED(ctx);
	ASSERT(airspyhf != NULL);       // input_airspyhf_open_device() runs first

	int r = airspyhf_set_freq(airspyhf, centerfreq);
	if(r != AIRSPYHF_SUCCESS) {
		fprintf(stderr, "Failed to set frequency to %u Hz: %s\n", centerfreq, airspyhf_strerror(r));
		_exit(1);
	}
	// The HF+ has a frequency correction setting of its own, calibrated in
	// parts per billion and kept in the device's flash, which libairspyhf
	// loads when the device is opened. Applying it re-tunes with the new value
	// in effect, and the sub-kHz remainder is taken care of in the library's
	// DSP, so there is no need to nudge the requested frequency by hand.
	// Leave the stored value alone unless --correction says otherwise -
	// overwriting it with a zero would throw away a calibrated device's own
	// setting for the duration of the run.
	if(correction != 0) {
		r = airspyhf_set_calibration(airspyhf, correction * 1000);
		if(r != AIRSPYHF_SUCCESS) {
			fprintf(stderr, "Failed to set frequency correction to %d ppm: %s\n",
					correction, airspyhf_strerror(r));
			_exit(1);
		}
		fprintf(stderr, "Center frequency set to %u Hz (corrected by %d ppm)\n",
				centerfreq, correction);
	} else {
		int32_t ppb = 0;
		if(airspyhf_get_calibration(airspyhf, &ppb) == AIRSPYHF_SUCCESS && ppb != 0) {
			fprintf(stderr, "Center frequency set to %u Hz (corrected by %" PRId32
					" ppb from the device's own calibration)\n", centerfreq, ppb);
		} else {
			fprintf(stderr, "Center frequency set to %u Hz\n", centerfreq);
		}
	}

	if(att != AIRSPYHF_ATT_UNSET && (att < 0 || att > AIRSPYHF_ATT_MAX)) {
		fprintf(stderr, "Attenuator setting %d is out of range (0-%d)\n", att, AIRSPYHF_ATT_MAX);
		_exit(1);
	}
	// The AGC and the attenuator are alternatives. Unless told otherwise, run
	// the AGC - but read an explicit attenuator setting as a request for
	// manual gain and keep the AGC out of the way.
	//
	// The gain commands were added to the HF+ firmware after the first
	// devices shipped, so they may fail on old firmware. That is fatal only
	// when it loses something the user asked for; a failure to re-assert a
	// default (AGC on, threshold low, LNA off - which is how the firmware
	// runs anyway) just gets a warning.
	bool gain_requested = agc != AIRSPYHF_AGC_UNSET || att != AIRSPYHF_ATT_UNSET;
	int agc_on = agc != AIRSPYHF_AGC_UNSET ? (agc > 0) : (att == AIRSPYHF_ATT_UNSET);
	r = airspyhf_set_hf_agc(airspyhf, agc_on ? 1 : 0);
	if(r != AIRSPYHF_SUCCESS) {
		fprintf(stderr, "%s to %s AGC: %s\n", gain_requested ? "Failed" : "Warning: unable",
				agc_on ? "enable" : "disable", airspyhf_strerror(r));
		if(gain_requested) {
			_exit(1);
		}
	} else {
		fprintf(stderr, "AGC %s\n", agc_on ? "enabled" : "disabled");
	}
	if(agc_on) {
		r = airspyhf_set_hf_agc_threshold(airspyhf, agc_threshold > 0 ? 1 : 0);
		if(r != AIRSPYHF_SUCCESS) {
			fprintf(stderr, "%s to set AGC threshold: %s\n",
					agc_threshold > 0 ? "Failed" : "Warning: unable", airspyhf_strerror(r));
			if(agc_threshold > 0) {
				_exit(1);
			}
		} else {
			fprintf(stderr, "AGC threshold set to %s\n", agc_threshold > 0 ? "high" : "low");
		}
	}
	if(att != AIRSPYHF_ATT_UNSET) {
		r = airspyhf_set_hf_att(airspyhf, (uint8_t)att);
		if(r != AIRSPYHF_SUCCESS) {
			fprintf(stderr, "Failed to set attenuator to %d: %s\n", att, airspyhf_strerror(r));
			_exit(1);
		}
		fprintf(stderr, "Attenuator set to %d (%d dB)\n", att, att * 6);
	}

	r = airspyhf_set_hf_lna(airspyhf, lna > 0 ? 1 : 0);
	if(r != AIRSPYHF_SUCCESS) {
		fprintf(stderr, "%s to %s LNA: %s\n", lna > 0 ? "Failed" : "Warning: unable",
				lna > 0 ? "enable" : "disable", airspyhf_strerror(r));
		if(lna > 0) {
			_exit(1);
		}
	} else {
		fprintf(stderr, "LNA %s\n", lna > 0 ? "enabled" : "disabled");
	}

	// Not every HF+ variant has a bias tee, so a failure here is only fatal
	// when the user has actually asked for it to be turned on.
	r = airspyhf_set_bias_tee(airspyhf, biast ? 1 : 0);
	if(r != AIRSPYHF_SUCCESS) {
		if(biast) {
			fprintf(stderr, "Failed to enable bias tee: %s\n", airspyhf_strerror(r));
			_exit(1);
		}
	} else {
		fprintf(stderr, "Bias tee %s\n", biast ? "enabled" : "disabled");
	}

	r = airspyhf_start(airspyhf, airspyhf_rx_callback, NULL);
	if(r != AIRSPYHF_SUCCESS) {
		fprintf(stderr, "Failed to start RX: %s\n", airspyhf_strerror(r));
		_exit(1);
	}
	fprintf(stderr, "Device started\n");
	while(do_exit == 0 && airspyhf_is_streaming(airspyhf)) {
		usleep(200000);
	}
	airspyhf_stop(airspyhf);
	airspyhf_close(airspyhf);
	airspyhf = NULL;
}

void input_airspyhf_cancel() {
	// NO-OP - input_airspyhf_start() releases the device once the do_exit flag
	// is raised. The RX callback returns an error as soon as it sees the flag,
	// which makes libairspyhf shut the transfer down.
}
