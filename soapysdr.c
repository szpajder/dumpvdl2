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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include "dumpvdl2.h"
#include "soapysdr.h"

extern int do_exit;

static void soapysdr_verbose_device_search() {
    size_t length;
    //enumerate devices
    SoapySDRKwargs *results = SoapySDRDevice_enumerate(NULL, &length);
    for (size_t i = 0; i < length; i++)
    {
        fprintf(stderr,"Found device #%d: ", (int)i);
        for (size_t j = 0; j < results[i].size; j++)
        {
            fprintf(stderr,"%s=%s, ", results[i].keys[j], results[i].vals[j]);
        }
        fprintf(stderr,"\n");
    }
    SoapySDRKwargsList_clear(results, length);
}

void soapysdr_init(vdl2_state_t *ctx, char *dev, char *antenna, int freq, float gain, int ppm_error, char* settings, char* gains_param) {
	SoapySDRKwargs args = {};
	soapysdr_verbose_device_search();
	SoapySDRKwargs dev_param = SoapySDRKwargs_fromString(dev);
	if (dev_param.size < 1) {
		fprintf(stderr,"Wrong device string.");
		_exit(1);
	} else {
		for (size_t i = 0; i < dev_param.size; i++) {
			SoapySDRKwargs_set(&args, dev_param.keys[i], dev_param.vals[i]);
		}
	}

    SoapySDRDevice *sdr = SoapySDRDevice_make(&args);
    SoapySDRKwargs_clear(&args);

    if (sdr == NULL)
    {
        fprintf(stderr, "SoapySDRDevice_make fail: %s\n", SoapySDRDevice_lastError());
        _exit(1);
    }

    if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, SOAPYSDR_RATE) != 0)
    {
        fprintf(stderr, "setSampleRate fail: %s\n", SoapySDRDevice_lastError());
        _exit(1);
    }
    if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, freq, NULL) != 0)
    {
        fprintf(stderr, "setFrequency fail: %s\n", SoapySDRDevice_lastError());
        _exit(1);
    }
    if (SoapySDRDevice_setFrequencyCorrection(sdr, SOAPY_SDR_RX, 0, (double)ppm_error) != 0)
    {
        fprintf(stderr, "setFrequencyCorrection fail: %s\n", SoapySDRDevice_lastError());
        _exit(1);
    }
    if (SoapySDRDevice_hasDCOffsetMode(sdr, SOAPY_SDR_RX, 0)) {
    	if (SoapySDRDevice_setDCOffsetMode(sdr, SOAPY_SDR_RX, 0, true) != 0) {
	        fprintf(stderr, "setDCOffsetMode fail: %s\n", SoapySDRDevice_lastError());
	        _exit(1);
    	}
    }

    if ((gain == SDR_AUTO_GAIN) && (SoapySDRDevice_hasGainMode(sdr, SOAPY_SDR_RX, 0))) {
    	if (SoapySDRDevice_setGainMode(sdr, SOAPY_SDR_RX, 0, true) != 0) {
	        fprintf(stderr, "setGainMode to automatic fail: %s\n", SoapySDRDevice_lastError());
	        _exit(1);
    	}
    } else {
    	if (strcmp(gains_param, "") == 0) {
	    	if (SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, 0, (double)gain) != 0)
		    {
		        fprintf(stderr, "setGain fail: %s\n", SoapySDRDevice_lastError());
		        _exit(1);
		    }
    	} else {
    		// Setup gain from string
			SoapySDRKwargs gains = SoapySDRKwargs_fromString(gains_param);
			if (gains.size < 1) {
		        fprintf(stderr, "Unable to parse gains string, must be a sequence of 'name1=value1,name2=value2,...'.\n");
		        _exit(1);
			}
			for (size_t i = 0; i < gains.size; i++) {
				SoapySDRDevice_setGainElement(sdr, SOAPY_SDR_RX, 0, gains.keys[i], atof(gains.vals[i]));
				debug_print("Set gain %s to %.2f\n",gains.keys[i], atof(gains.vals[i]));
				double gain_value = SoapySDRDevice_getGainElement(sdr, SOAPY_SDR_RX, 0, gains.keys[i]);
				fprintf(stderr,"Set gain %s to %.2f\n",gains.keys[i], gain_value);

		    }
		    SoapySDRKwargs_clear(&gains);
		}
	}

	if (SoapySDRDevice_setAntenna(sdr, SOAPY_SDR_RX, 0, antenna) != 0) {
		fprintf(stderr, "setAntenna fail: %s\n", SoapySDRDevice_lastError());
		_exit(1);
	}
	// Check parameters
	fprintf(stderr, "Antenna : %s\n", SoapySDRDevice_getAntenna(sdr, SOAPY_SDR_RX, 0));

    // Setup settings
    if (strcmp(settings, "") != 0) {
		SoapySDRKwargs settings_param = SoapySDRKwargs_fromString(settings);
		if (settings_param.size < 1) {
	        fprintf(stderr, "Unable to parse settings string, must be a sequence of 'name1=value1,name2=value2,...'.\n");
	        _exit(1);
		}
		for (size_t i = 0; i < settings_param.size; i++) {
			SoapySDRDevice_writeSetting(sdr, settings_param.keys[i], settings_param.vals[i]);
			debug_print("Set param %s to %s\n",settings_param.keys[i], settings_param.vals[i]);
			char *setting_value = SoapySDRDevice_readSetting(sdr, settings_param.keys[i]);
			fprintf(stderr, "Setting %s is %s => %s\n",settings_param.keys[i], setting_value, (strcmp(settings_param.vals[i],setting_value) == 0) ? "done" : "failed");

	    }
	    SoapySDRKwargs_clear(&settings_param);
	}

    size_t elemsize = SoapySDR_formatToSize(SOAPY_SDR_CS16);
    int16_t *buffer = XCALLOC(SOAPYSDR_SAMPLE_PER_BUFFER, elemsize);
    unsigned char *ring_buffer = XCALLOC(SOAPYSDR_BUFSIZE * SOAPYSDR_BUFCNT, sizeof(short));
    unsigned char *send_buffer = XCALLOC(SOAPYSDR_BUFSIZE, sizeof(short));
    sbuf = XCALLOC(SOAPYSDR_BUFSIZE, sizeof(float));

    SoapySDRStream *rxStream;
    if (SoapySDRDevice_setupStream(sdr, &rxStream, SOAPY_SDR_RX, SOAPY_SDR_CS16, NULL, 0, NULL) != 0)
    {
        fprintf(stderr, "setupStream fail: %s\n", SoapySDRDevice_lastError());
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
			if (ring_index + r * elemsize < SOAPYSDR_BUFSIZE * SOAPYSDR_BUFCNT * sizeof(short)) {
				for (int i=0; i< iq_count; i++) {
					// copy low / high part
					ring_buffer[ring_index++] = buffer[i] & 0xff;
					ring_buffer[ring_index++] = (buffer[i] >> 8) & 0xff;
				}
			} else {
				int counth = SOAPYSDR_BUFSIZE * SOAPYSDR_BUFCNT * sizeof(short) - ring_index;
				int buffer_index = 0;
				for (int i=0; i< counth / sizeof(short); i++) {
					// copy low / high part
					ring_buffer[ring_index++] = buffer[buffer_index] & 0xff;
					ring_buffer[ring_index++] = (buffer[buffer_index] >> 8) & 0xff;
					buffer_index++;
				}
				int countl = iq_count * sizeof(short) - counth; 
				ring_index = 0;
				for (int i=0; i< countl / sizeof(short); i++) {
					// copy low / high part
					ring_buffer[ring_index++] = buffer[buffer_index] & 0xff;
					ring_buffer[ring_index++] = (buffer[buffer_index] >> 8) & 0xff;
					buffer_index++;
				}
			}

			// Store * elemsize
			if (ring_index < last_send_index) {
			  int size = ring_index + SOAPYSDR_BUFSIZE * SOAPYSDR_BUFCNT * sizeof(short) - last_send_index;
			  if (size >= buf_elem_size) {
			     // copy to trunc buffer
			     int send_index = 0;
			     for (int i=0; i< buf_elem_size; i++) {
				// copy low / high part
				send_buffer[send_index++] = ring_buffer[last_send_index++];
				if (last_send_index >= SOAPYSDR_BUFSIZE * SOAPYSDR_BUFCNT * sizeof(short)) {
				  last_send_index = 0;
				}
			     }
			     process_buf_short(&send_buffer[0], buf_elem_size, NULL);
			  }
			} else if ((ring_index - last_send_index) >= buf_elem_size)  {
			  process_buf_short(&ring_buffer[ring_index - buf_elem_size], buf_elem_size, NULL);
			  last_send_index = ring_index;
			}
	}
	//shutdown the stream
    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0); //stop streaming
    SoapySDRDevice_closeStream(sdr, rxStream);
    //cleanup device handle
    SoapySDRDevice_unmake(sdr);
}

void soapysdr_cancel() {
}
