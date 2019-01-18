/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
 *  Copyright (c) 2017-2019 Tomasz Lemiech <szpajder@gmail.com>
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
#include <stdio.h>			// fprintf()
#include <stdlib.h>			// atof(), free()
#include <string.h>			// strcmp()
#include <unistd.h>			// _exit(), usleep()
#include <SoapySDR/Types.h>		// SoapySDRKwargs_*
#include <SoapySDR/Device.h>		// SoapySDRStream, SoapySDRDevice_*
#include <SoapySDR/Formats.h>		// SOAPY_SDR_CS16, SoapySDR_formatToSize()
#include "dumpvdl2.h"			// vdl2_state_t, do_exit, XFREE()
#include "soapysdr.h"

static void soapysdr_verbose_device_search() {
	size_t length;
	// enumerate devices
	SoapySDRKwargs *results = SoapySDRDevice_enumerate(NULL, &length);
	for(size_t i = 0; i < length; i++) {
		fprintf(stderr, "Found device #%d:\n", (int)i);
		for(size_t j = 0; j < results[i].size; j++) {
			fprintf(stderr, "  %s = %s\n", results[i].keys[j], results[i].vals[j]);
		}
	}
	SoapySDRKwargsList_clear(results, length);
}

void soapysdr_init(vdl2_state_t *ctx, char *dev, char *antenna, int freq, float gain,
int ppm_error, char* settings, char* gains_param) {
// -Wunused-parameter
	(void)ctx;
	soapysdr_verbose_device_search();

	SoapySDRDevice *sdr = SoapySDRDevice_makeStrArgs(dev);
	if(sdr == NULL) {
		fprintf(stderr, "Could not open SoapySDR device '%s': %s\n", dev, SoapySDRDevice_lastError());
		_exit(1);
	}
	if(SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, SOAPYSDR_RATE) != 0) {
		fprintf(stderr, "setSampleRate failed: %s\n", SoapySDRDevice_lastError());
		_exit(1);
	}
	if(SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, freq, NULL) != 0) {
		fprintf(stderr, "setFrequency failed: %s\n", SoapySDRDevice_lastError());
		_exit(1);
	}
	if(SoapySDRDevice_setFrequencyCorrection(sdr, SOAPY_SDR_RX, 0, (double)ppm_error) != 0) {
		fprintf(stderr, "setFrequencyCorrection failed: %s\n", SoapySDRDevice_lastError());
		_exit(1);
	}
	if(SoapySDRDevice_hasDCOffsetMode(sdr, SOAPY_SDR_RX, 0)) {
		if(SoapySDRDevice_setDCOffsetMode(sdr, SOAPY_SDR_RX, 0, true) != 0) {
			fprintf(stderr, "setDCOffsetMode failed: %s\n", SoapySDRDevice_lastError());
			_exit(1);
		}
	}

// If both --gain and --soapy-gain are present, the latter takes precedence.
	if(gains_param != NULL) {
		SoapySDRKwargs gains = SoapySDRKwargs_fromString(gains_param);
		if(gains.size < 1) {
			fprintf(stderr, "Unable to parse gains string, "
				"must be a sequence of 'name1=value1,name2=value2,...'.\n");
			_exit(1);
		}
		for(size_t i = 0; i < gains.size; i++) {
			SoapySDRDevice_setGainElement(sdr, SOAPY_SDR_RX, 0, gains.keys[i], atof(gains.vals[i]));
			debug_print("Set gain %s to %.2f\n", gains.keys[i], atof(gains.vals[i]));
			double gain_value = SoapySDRDevice_getGainElement(sdr, SOAPY_SDR_RX, 0, gains.keys[i]);
			fprintf(stderr, "Gain element %s set to %.2f dB\n", gains.keys[i], gain_value);

		}
		SoapySDRKwargs_clear(&gains);
		XFREE(gains_param);
	} else {
		if(gain == SDR_AUTO_GAIN) {
			if(SoapySDRDevice_hasGainMode(sdr, SOAPY_SDR_RX, 0) == false) {
				fprintf(stderr, "Selected device does not support auto gain. "
					"Please specify manual gain with --gain or --soapy-gain option\n");
				_exit(1);
			}
			if(SoapySDRDevice_setGainMode(sdr, SOAPY_SDR_RX, 0, true) != 0) {
				fprintf(stderr, "Could not enable auto gain: %s\n", SoapySDRDevice_lastError());
				_exit(1);
			}
			fprintf(stderr, "Auto gain enabled\n");
		} else {
			if(SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, 0, (double)gain) != 0) {
				fprintf(stderr, "Could not set gain: %s\n", SoapySDRDevice_lastError());
				_exit(1);
			}
			fprintf(stderr, "Gain set to %.2f dB\n", gain);
		}
	}

	if(antenna != NULL) {
		if(SoapySDRDevice_setAntenna(sdr, SOAPY_SDR_RX, 0, antenna) != 0) {
			fprintf(stderr, "Could not select antenna %s: %s\n", antenna, SoapySDRDevice_lastError());
			_exit(1);
		}
		XFREE(antenna);
	}
	fprintf(stderr, "Antenna: %s\n", SoapySDRDevice_getAntenna(sdr, SOAPY_SDR_RX, 0));

	if(settings != NULL) {
		SoapySDRKwargs settings_param = SoapySDRKwargs_fromString(settings);
		if(settings_param.size < 1) {
			fprintf(stderr, "Unable to parse settings string, must be a sequence of 'name1=value1,name2=value2,...'.\n");
			_exit(1);
		}
		for(size_t i = 0; i < settings_param.size; i++) {
			SoapySDRDevice_writeSetting(sdr, settings_param.keys[i], settings_param.vals[i]);
			debug_print("Set param %s to %s\n", settings_param.keys[i], settings_param.vals[i]);
			char *setting_value = SoapySDRDevice_readSetting(sdr, settings_param.keys[i]);
			fprintf(stderr, "Setting %s is %s => %s\n", settings_param.keys[i], setting_value,
				(strcmp(settings_param.vals[i], setting_value) == 0) ? "done" : "failed");
		}
		SoapySDRKwargs_clear(&settings_param);
		XFREE(settings);
	}

	size_t elemsize = SoapySDR_formatToSize(SOAPY_SDR_CS16);
	int16_t *buffer = XCALLOC(SOAPYSDR_SAMPLE_PER_BUFFER, elemsize);
	unsigned char *ring_buffer = XCALLOC(SOAPYSDR_BUFSIZE * SOAPYSDR_BUFCNT, sizeof(short));
	unsigned char *send_buffer = XCALLOC(SOAPYSDR_BUFSIZE, sizeof(short));
	sbuf = XCALLOC(SOAPYSDR_BUFSIZE, sizeof(float));

	SoapySDRStream *rxStream;
	if(SoapySDRDevice_setupStream(sdr, &rxStream, SOAPY_SDR_RX, SOAPY_SDR_CS16, NULL, 0, NULL) != 0) {
		fprintf(stderr, "setupStream failed: %s\n", SoapySDRDevice_lastError());
		_exit(1);
	}
	SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0);
	usleep(100000);

	// Read input samples
	long ring_index = 0;
	long buf_elem_size = SOAPYSDR_BUFSIZE * sizeof(short);
	long last_send_index = 0;
	while (!do_exit) {
		void *buffs[] = {buffer};
		int flags = 0;
		long long timeNs = 0;
		long timeoutNs = 1000000;
		int r;
		r = SoapySDRDevice_readStream(sdr, rxStream, buffs, SOAPYSDR_SAMPLE_PER_BUFFER, &flags, &timeNs, timeoutNs);
		int iq_count = r * 2;
		// copy to ring_buffer
		// Ring_index is on unsigned char
		if(ring_index + r * elemsize < SOAPYSDR_BUFSIZE * SOAPYSDR_BUFCNT * sizeof(short)) {
			for(int i = 0; i< iq_count; i++) {
				// copy low / high part
				ring_buffer[ring_index++] = buffer[i] & 0xff;
				ring_buffer[ring_index++] = (buffer[i] >> 8) & 0xff;
			}
		} else {
			int counth = SOAPYSDR_BUFSIZE * SOAPYSDR_BUFCNT * sizeof(short) - ring_index;
			int buffer_index = 0;
			for(int i = 0; i < counth / sizeof(short); i++) {
				// copy low / high part
				ring_buffer[ring_index++] = buffer[buffer_index] & 0xff;
				ring_buffer[ring_index++] = (buffer[buffer_index] >> 8) & 0xff;
				buffer_index++;
			}
			int countl = iq_count * sizeof(short) - counth;
			ring_index = 0;
			for(int i = 0; i< countl / sizeof(short); i++) {
				// copy low / high part
				ring_buffer[ring_index++] = buffer[buffer_index] & 0xff;
				ring_buffer[ring_index++] = (buffer[buffer_index] >> 8) & 0xff;
				buffer_index++;
			}
		}

		// Store * elemsize
		if(ring_index < last_send_index) {
			int size = ring_index + SOAPYSDR_BUFSIZE * SOAPYSDR_BUFCNT * sizeof(short) - last_send_index;
			if(size >= buf_elem_size) {
			// copy to trunc buffer
				int send_index = 0;
				for(int i = 0; i< buf_elem_size; i++) {
			// copy low / high part
					send_buffer[send_index++] = ring_buffer[last_send_index++];
					if(last_send_index >= SOAPYSDR_BUFSIZE * SOAPYSDR_BUFCNT * sizeof(short)) {
						last_send_index = 0;
					}
				}
				process_buf_short(&send_buffer[0], buf_elem_size, NULL);
			}
		} else if((ring_index - last_send_index) >= buf_elem_size) {
			process_buf_short(&ring_buffer[ring_index - buf_elem_size], buf_elem_size, NULL);
			last_send_index = ring_index;
		}
	}
	SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
	SoapySDRDevice_closeStream(sdr, rxStream);
	SoapySDRDevice_unmake(sdr);
}

void soapysdr_cancel() {
}
