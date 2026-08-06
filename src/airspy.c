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
#include <libairspy/airspy.h>
#include "dumpvdl2.h"           // do_exit, process_buf_short(), XCALLOC(), XFREE(), ASSERT()
#include "airspy.h"

// NOTE: libairspy owns the airspy_* namespace, so the entry points here are
// named to stay clear of it - airspy_start() rather than airspy_init(), which
// is a libairspy function.

static struct airspy_device *airspy = NULL;
static uint64_t last_dropped_samples;

static void airspy_verbose_device_search(uint64_t *serials, int count) {
	fprintf(stderr, "Found %d device(s):\n", count);
	for(int i = 0; i < count; i++) {
		fprintf(stderr, "  %d:  SN: %016" PRIx64 "\n", i, serials[i]);
	}
	fprintf(stderr, "\n");
}

// Opens the Airspy device selected by dev (a device index or a serial number,
// possibly abbreviated) and decides which sample rate to run it at. Returns
// the chosen rate, which the caller needs in order to set up the resampler
// before the demodulator threads are started.
uint32_t airspy_open_device(char *dev, uint32_t sample_rate) {
	int r = airspy_init();
	if(r != AIRSPY_SUCCESS) {
		fprintf(stderr, "airspy_init() failed: %s\n", airspy_error_name(r));
		_exit(1);
	}
	uint64_t serials[AIRSPY_MAX_DEVICES];
	int count = airspy_list_devices(serials, AIRSPY_MAX_DEVICES);
	if(count < 1) {
		fprintf(stderr, "No supported devices found.\n");
		_exit(1);
	}
	if(count > AIRSPY_MAX_DEVICES) {
		count = AIRSPY_MAX_DEVICES;
	}
	airspy_verbose_device_search(serials, count);

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
	r = airspy_open_sn(&airspy, serial);
	if(r != AIRSPY_SUCCESS) {
		fprintf(stderr, "Failed to open Airspy device %016" PRIx64 ": %s\n",
				serial, airspy_error_name(r));
		_exit(1);
	}
	fprintf(stderr, "Using device with SN: %016" PRIx64 "\n", serial);

	// Read the list of rates the device supports. No Airspy rate is a multiple
	// of SYMBOL_RATE * SPS, so whichever one gets picked, the sample stream is
	// going to be converted (see --resampler).
	uint32_t rate_count = 0;
	r = airspy_get_samplerates(airspy, &rate_count, 0);
	if(r != AIRSPY_SUCCESS || rate_count < 1) {
		fprintf(stderr, "Failed to read supported sample rates: %s\n", airspy_error_name(r));
		_exit(1);
	}
	uint32_t *rates = XCALLOC(rate_count, sizeof(uint32_t));
	r = airspy_get_samplerates(airspy, rates, rate_count);
	if(r != AIRSPY_SUCCESS) {
		fprintf(stderr, "Failed to read supported sample rates: %s\n", airspy_error_name(r));
		_exit(1);
	}
	uint32_t chosen = 0;
	if(sample_rate == 0) {
		// Default to the lowest supported rate - it is the cheapest one to
		// process and it still covers the whole VDL2 band.
		chosen = rates[0];
		for(uint32_t i = 1; i < rate_count; i++) {
			if(rates[i] < chosen) {
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
	r = airspy_set_samplerate(airspy, chosen);
	if(r != AIRSPY_SUCCESS) {
		fprintf(stderr, "Failed to set sample rate to %u: %s\n", chosen, airspy_error_name(r));
		_exit(1);
	}
	return chosen;
}

static int airspy_rx_callback(airspy_transfer *transfer) {
	if(do_exit) {
		return -1;
	}
	if(transfer->dropped_samples != last_dropped_samples) {
		fprintf(stderr, "Warning: dropped %" PRIu64 " samples "
				"(sample rate too high for this machine?)\n",
				transfer->dropped_samples - last_dropped_samples);
		last_dropped_samples = transfer->dropped_samples;
	}
	// AIRSPY_SAMPLE_INT16_IQ delivers two int16 values per sample, which is
	// what process_buf_short() expects. Block sizes are decided by libairspy
	// and may vary, so sbuf is not preallocated here - process_buf_short()
	// sizes it at the only point where doing so is safe.
	process_buf_short(transfer->samples, (uint32_t)transfer->sample_count * 2 * sizeof(int16_t), NULL);
	return 0;
}

void airspy_start(vdl2_state_t *ctx, uint32_t centerfreq, int linearity_gain, int sensitivity_gain,
		int lna_gain, int mixer_gain, int vga_gain, int lna_agc, int mixer_agc,
		int correction, int biast, int packing) {
	UNUSED(ctx);
	ASSERT(airspy != NULL);         // airspy_open_device() runs first
	int r = airspy_set_sample_type(airspy, AIRSPY_SAMPLE_INT16_IQ);
	if(r != AIRSPY_SUCCESS) {
		fprintf(stderr, "Failed to set sample type: %s\n", airspy_error_name(r));
		_exit(1);
	}
	r = airspy_set_packing(airspy, packing ? 1 : 0);
	if(r != AIRSPY_SUCCESS) {
		fprintf(stderr, "Failed to %s sample packing: %s\n",
				packing ? "enable" : "disable", airspy_error_name(r));
		_exit(1);
	}
	// The Airspy has no frequency correction setting of its own, so apply it
	// to the tuned frequency instead.
	uint32_t tuned_freq = (uint32_t)((double)centerfreq * (1.0 + (double)correction / 1e6));
	r = airspy_set_freq(airspy, tuned_freq);
	if(r != AIRSPY_SUCCESS) {
		fprintf(stderr, "Failed to set frequency to %u Hz: %s\n", tuned_freq, airspy_error_name(r));
		_exit(1);
	}
	if(correction != 0) {
		fprintf(stderr, "Center frequency set to %u Hz (%u Hz corrected by %d ppm)\n",
				tuned_freq, centerfreq, correction);
	} else {
		fprintf(stderr, "Center frequency set to %u Hz\n", tuned_freq);
	}

	if(linearity_gain != AIRSPY_GAIN_UNSET) {
		r = airspy_set_linearity_gain(airspy, (uint8_t)linearity_gain);
		if(r != AIRSPY_SUCCESS) {
			fprintf(stderr, "Failed to set linearity gain: %s\n", airspy_error_name(r));
			_exit(1);
		}
		fprintf(stderr, "Linearity gain set to %d\n", linearity_gain);
	} else if(sensitivity_gain != AIRSPY_GAIN_UNSET) {
		r = airspy_set_sensitivity_gain(airspy, (uint8_t)sensitivity_gain);
		if(r != AIRSPY_SUCCESS) {
			fprintf(stderr, "Failed to set sensitivity gain: %s\n", airspy_error_name(r));
			_exit(1);
		}
		fprintf(stderr, "Sensitivity gain set to %d\n", sensitivity_gain);
	} else if(lna_gain != AIRSPY_GAIN_UNSET || mixer_gain != AIRSPY_GAIN_UNSET ||
			vga_gain != AIRSPY_GAIN_UNSET || lna_agc > 0 || mixer_agc > 0) {
		// Manual gain distribution. Anything left unset stays at 0, which is
		// what the device powers up with.
		r = airspy_set_lna_agc(airspy, lna_agc > 0 ? 1 : 0);
		r |= airspy_set_mixer_agc(airspy, mixer_agc > 0 ? 1 : 0);
		if(lna_agc <= 0) {
			r |= airspy_set_lna_gain(airspy, (uint8_t)(lna_gain != AIRSPY_GAIN_UNSET ? lna_gain : 0));
		}
		if(mixer_agc <= 0) {
			r |= airspy_set_mixer_gain(airspy, (uint8_t)(mixer_gain != AIRSPY_GAIN_UNSET ? mixer_gain : 0));
		}
		r |= airspy_set_vga_gain(airspy, (uint8_t)(vga_gain != AIRSPY_GAIN_UNSET ? vga_gain : 0));
		if(r != AIRSPY_SUCCESS) {
			fprintf(stderr, "Failed to set gains: %s\n", airspy_error_name(r));
			_exit(1);
		}
		fprintf(stderr, "Gains set to: LNA %d%s, mixer %d%s, VGA %d\n",
				lna_gain != AIRSPY_GAIN_UNSET ? lna_gain : 0, lna_agc > 0 ? " (AGC)" : "",
				mixer_gain != AIRSPY_GAIN_UNSET ? mixer_gain : 0, mixer_agc > 0 ? " (AGC)" : "",
				vga_gain != AIRSPY_GAIN_UNSET ? vga_gain : 0);
	} else {
		// Leaving every gain at 0 would make the receiver deaf, which is a
		// confusing way to start up. Pick a value which works for VHF airband.
		r = airspy_set_linearity_gain(airspy, AIRSPY_DEFAULT_LINEARITY_GAIN);
		if(r != AIRSPY_SUCCESS) {
			fprintf(stderr, "Failed to set linearity gain: %s\n", airspy_error_name(r));
			_exit(1);
		}
		fprintf(stderr, "Gain not specified - using linearity gain %d "
				"(see --linearity-gain, --sensitivity-gain)\n", AIRSPY_DEFAULT_LINEARITY_GAIN);
	}

	r = airspy_set_rf_bias(airspy, biast ? 1 : 0);
	if(r != AIRSPY_SUCCESS) {
		fprintf(stderr, "Failed to set bias tee: %s\n", airspy_error_name(r));
		_exit(1);
	}
	fprintf(stderr, "Bias tee %s\n", biast ? "enabled" : "disabled");

	r = airspy_start_rx(airspy, airspy_rx_callback, NULL);
	if(r != AIRSPY_SUCCESS) {
		fprintf(stderr, "Failed to start RX: %s\n", airspy_error_name(r));
		_exit(1);
	}
	fprintf(stderr, "Device started\n");
	while(do_exit == 0 && airspy_is_streaming(airspy) == AIRSPY_TRUE) {
		usleep(200000);
	}
	airspy_stop_rx(airspy);
	airspy_close(airspy);
	airspy = NULL;
	airspy_exit();
}

void airspy_cancel() {
	// NO-OP - airspy_start() releases the device once the do_exit flag is
	// raised. The RX callback returns an error as soon as it sees the flag,
	// which makes libairspy shut the transfer down.
}
