/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017 Fabrice Crohas <fcrohas@gmail.com>
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
#include <stdio.h>              // fprintf
#include <stdlib.h>             // calloc, strtol
#include <string.h>             // strcmp
#include <unistd.h>             // _exit, usleep
#include <mirsdrapi-rsp.h>
#include "dumpvdl2.h"           // sbuf
#include "sdrplay.h"

static int initialized = 0;

#define NUM_LNA_STATES 10       // Max number of LNA states of all hw types
static int lnaGRtables[NUM_HW_TYPES][NUM_LNA_STATES] = {
	[HW_RSP1]   = { 0, 24, 19, 43, 0, 0, 0, 0, 0, 0 },
	[HW_RSP2]   = { 0, 10, 15, 21, 24, 34, 39, 45, 64, 0 },
	[HW_RSP1A]  = { 0, 6, 12, 18, 20, 26, 32, 38, 57, 62 },
	[HW_RSPDUO] = { 0, 6, 12, 18, 20, 26, 32, 38, 57, 62 }
};
static int num_lnaGRs[NUM_HW_TYPES] = {
	[HW_RSP1] = 4,
	[HW_RSP2] = 9,
	[HW_RSP1A] = 10,
	[HW_RSPDUO] = 10
};
static char *hw_descr[NUM_HW_TYPES] = {
	[HW_RSP1] = "RSP1",
	[HW_RSP2] = "RSP2",
	[HW_RSP1A] = "RSP1A",
	[HW_RSPDUO] = "RSPduo"
};

static void sdrplay_streamCallback(short *xi, short *xq, unsigned int firstSampleNum, int grChanged,
		int rfChanged, int fsChanged, unsigned int numSamples, unsigned int reset, unsigned int hwRemoved, void *cbContext) {
	UNUSED(firstSampleNum);
	UNUSED(grChanged);
	UNUSED(rfChanged);
	UNUSED(fsChanged);
	UNUSED(reset);
	UNUSED(hwRemoved);
	int i, j, count1, count2, new_buf_flag;
	int end, input_index;
	CAST_PTR(SDRPlay, sdrplay_ctx_t *, cbContext);
	if(numSamples == 0) {
		return;
	}
	unsigned char *dptr = SDRPlay->sdrplay_data;
	// data_index counts samples
	// numSamples counts I/Q sample pairs
	end = SDRPlay->data_index + (numSamples * 2);
	count2 = end - (ASYNC_BUF_SIZE * ASYNC_BUF_NUMBER);
	if(count2 < 0) count2 = 0;              // count2 is samples wrapping around to start of buf
	count1 = (numSamples * 2) - count2;     // count1 is samples fitting before the end of buf

	// flag is set if this packet takes us past a multiple of ASYNC_BUF_SIZE
	new_buf_flag = (SDRPlay->data_index / ASYNC_BUF_SIZE == end / ASYNC_BUF_SIZE ? 0 : 1);

	// now interleave data from I/Q into circular buffer
	input_index = 0;
	for (i = 0, j = SDRPlay->data_index * sizeof(short); i < count1 / 2; i++) {
		// Copy I low / high part
		dptr[j++] = xi[input_index] & 0xff;
		dptr[j++] = (xi[input_index] >> 8) & 0xff;
		// Copy Q low / high part
		dptr[j++] = xq[input_index] & 0xff;
		dptr[j++] = (xq[input_index] >> 8 ) & 0xff;
		input_index++;
	}
	SDRPlay->data_index += count1;

	if(SDRPlay->data_index >= ASYNC_BUF_SIZE * ASYNC_BUF_NUMBER) {
		SDRPlay->data_index = 0;            // pointer back to start of buffer
	}

	// insert remaining samples at the start of the buffer
	for (i = 0, j = SDRPlay->data_index * sizeof(short); i < count2 / 2; i++) {
		// Copy I low / high part
		dptr[j++] = xi[input_index] & 0xff;
		dptr[j++] = (xi[input_index] >> 8) & 0xff;
		// Copy Q low / high part
		dptr[j++] = xq[input_index] & 0xff;
		dptr[j++] = (xq[input_index] >> 8 ) & 0xff;
		input_index++;
	}
	SDRPlay->data_index += count2;

	// send ASYNC_BUF_SIZE samples downstream, if available
	if(new_buf_flag) {
		// go back by one buffer length, then round down further to start of buffer
		end = SDRPlay->data_index - ASYNC_BUF_SIZE;
		if(end < 0) end += ASYNC_BUF_SIZE * ASYNC_BUF_NUMBER;
		end -= end % ASYNC_BUF_SIZE;
		process_buf_short(&SDRPlay->sdrplay_data[end * sizeof(short)], ASYNC_BUF_SIZE * sizeof(short), NULL);
	}
}

static void sdrplay_gainCallback(unsigned int gRdB, unsigned int lnaGRdB, void *cbContext) {
	UNUSED(gRdB);
	UNUSED(lnaGRdB);
	UNUSED(cbContext);
	debug_print(D_SDR, "Gain change: gRdb=%u lnaGRdB=%u \n", gRdB, lnaGRdB);
}

static int sdrplay_verbose_device_search(char * const dev, sdrplay_hw_type *hw_type) {
	*hw_type = HW_UNKNOWN;
	int devIdx = -1;
	if(dev == NULL) {
		return -1;
	}
	mir_sdr_DeviceT devices[4];
	unsigned int numDevs;

	mir_sdr_ErrT err = mir_sdr_GetDevices(devices, &numDevs, 4);
	if(err != mir_sdr_Success) {
		fprintf(stderr, "Unable to enumerate connected SDRPlay devices, error %d\n", err);
		return -1;
	}
	if(numDevs < 1) {
		fprintf(stderr, "No RSP devices found\n");
		return -1;
	}

	fprintf(stderr, "\nFound %d device(s):\n", numDevs);
	for(unsigned int i = 0; i < numDevs; i++) {
		fprintf(stderr, "  %s %u:  SN: %s\n",
				devices[i].devAvail ? "        " : "(in use)",
				i,
				devices[i].SerNo != NULL ? devices[i].SerNo : "<none>"
			   );
	}
	fprintf(stderr, "\n");

	// Does the string look like a raw ID number?
	char *endptr = dev;
	long num = strtol(dev, &endptr, 0);
	if(endptr[0] == '\0' && num >= 0 && num < numDevs) {
		devIdx = (unsigned int)num;
		goto dev_found;
	}

	// Does the string match a serial number?
	for (unsigned int i = 0; i < numDevs; i++) {
		if(devices[i].SerNo == NULL) {
			continue;
		} else if(strcmp(dev, devices[i].SerNo) != 0) {
			continue;
		}
		devIdx = i;
		goto dev_found;
	}

	fprintf(stderr, "No matching devices found\n");
	return -1;

dev_found:
	if(devices[devIdx].devAvail != 1) {
		fprintf(stderr, "Selected device #%d is not available\n", devIdx);
		return -1;
	}
	if(devices[devIdx].hwVer == 1) {
		*hw_type = HW_RSP1;
	} else if(devices[devIdx].hwVer == 2) {
		*hw_type = HW_RSP2;
	} else if(devices[devIdx].hwVer == 3) {
		*hw_type = HW_RSPDUO;
	} else if(devices[devIdx].hwVer > 253) {
		*hw_type = HW_RSP1A;
	} else {
		fprintf(stderr, "Selected device #%d is unsupported: hardware version %d\n",
				devIdx, devices[devIdx].hwVer);
		return -1;
	}

	fprintf(stderr, "Selected device #%d (type: %s SN: %s)\n",
			devIdx,
			hw_descr[*hw_type],
			(devices[devIdx].SerNo != NULL ? devices[devIdx].SerNo : "unknown")
		   );
	return devIdx;
}

void sdrplay_init(vdl2_state_t * const ctx, char * const dev, char * const antenna,
		uint32_t const freq, int const gr, int const ppm_error, int const enable_biast,
		int const enable_notch_filter, int enable_agc, int tuner) {
	UNUSED(ctx);

	mir_sdr_ErrT err;
	float ver;
	sdrplay_ctx_t SDRPlay;
	sdrplay_hw_type hw_type = HW_UNKNOWN;

	err = mir_sdr_ApiVersion(&ver);
	if((err != mir_sdr_Success) || (ver != MIR_SDR_API_VERSION)) {
		fprintf(stderr, "Incorrect API version %f\n", ver);
		_exit(1);
	}
	fprintf(stderr, "Using SDRPlay API version %.3f\n", ver);
#ifdef DEBUG
	mir_sdr_DebugEnable(1);
#endif
	int devIdx = sdrplay_verbose_device_search(dev, &hw_type);
	if(devIdx < 0) {
		_exit(1);
	}
	err = mir_sdr_SetDeviceIdx(devIdx);
	if(err != mir_sdr_Success) {
		fprintf(stderr, "Unable to select device #%d, error %d\n", devIdx, err);
		_exit(1);
	}

	if(hw_type == HW_RSP2) {
		if(enable_biast) {
			err = mir_sdr_RSPII_BiasTControl(1);
			if(err != mir_sdr_Success) {
				fprintf(stderr, "Unable to activate Bias-T, error %d\n", err);
				_exit(1);
			}
			fprintf(stderr, "Bias-T activated\n");
		}

		if(strcmp(antenna, "A") == 0) {
			err = mir_sdr_RSPII_AntennaControl(mir_sdr_RSPII_ANTENNA_A);
		} else if(strcmp(antenna, "B") == 0) {
			err = mir_sdr_RSPII_AntennaControl(mir_sdr_RSPII_ANTENNA_B);
		} else {
			fprintf(stderr, "Invalid antenna port specified\n");
			_exit(1);
		}
		if(err != mir_sdr_Success) {
			fprintf(stderr, "Unable to select antenna port %s, error %d\n", antenna, err);
			_exit(1);
		}
		fprintf(stderr, "Using antenna port %s\n", antenna);

		if(enable_notch_filter) {
			err = mir_sdr_RSPII_RfNotchEnable(1);
			if(err != mir_sdr_Success) {
				fprintf(stderr, "Unable to activate RF notch filter, error %d\n", err);
				_exit(1);
			}
			fprintf(stderr, "RF notch filter enabled\n");
		}
	} else if(hw_type == HW_RSP1A) {
		if(enable_biast) {
			err = mir_sdr_rsp1a_BiasT(1);
			if(err != mir_sdr_Success) {
				fprintf(stderr, "Unable to activate Bias-T, error %d\n", err);
				_exit(1);
			}
			fprintf(stderr, "Bias-T activated\n");
		}
		if(enable_notch_filter) {
			err = mir_sdr_rsp1a_BroadcastNotch(1);
			if(err != mir_sdr_Success) {
				fprintf(stderr, "Unable to activate broadcast notch filter, error %d\n", err);
				_exit(1);
			}
			fprintf(stderr, "Broadcast notch filter enabled\n");
		}
	} else if(hw_type == HW_RSPDUO) {
		err = mir_sdr_rspDuo_TunerSel(tuner);
		if(err != mir_sdr_Success) {
			fprintf(stderr, "Unable to select tuner %d, error %d\n", tuner, err);
			_exit(1);
		}
		fprintf(stderr, "RSPduo: selected tuner %d\n", tuner);
		if(enable_biast) {
			err = mir_sdr_rspDuo_BiasT(1);
			if(err != mir_sdr_Success) {
				fprintf(stderr, "Unable to activate Bias-T, error %d\n", err);
				_exit(1);
			}
			fprintf(stderr, "Bias-T activated\n");
		}
		if(enable_notch_filter) {
			err = mir_sdr_rspDuo_BroadcastNotch(1);
			if(err != mir_sdr_Success) {
				fprintf(stderr, "Unable to activate broadcast notch filter, error %d\n", err);
				_exit(1);
			}
			fprintf(stderr, "Broadcast notch filter enabled\n");
		}
	}

	err = mir_sdr_DCoffsetIQimbalanceControl(1, 0);
	if(err != mir_sdr_Success) {
		fprintf(stderr, "Failed to set DC/IQ correction, error %d\n", err);
		_exit(1);
	}

	err = mir_sdr_SetPpm(ppm_error);
	if(err != mir_sdr_Success) {
		fprintf(stderr, "Unable to set frequency correction, error %d\n", err);
		_exit(1);
	}
	fprintf(stderr, "Frequency correction set to %d ppm\n", ppm_error);

	SDRPlay.sdrplay_data = XCALLOC(ASYNC_BUF_SIZE * ASYNC_BUF_NUMBER, sizeof(short));
	sbuf = XCALLOC(ASYNC_BUF_SIZE, sizeof(float));

	int gRdBsystem = gr;
	if(gr == SDR_AUTO_GAIN) {
		gRdBsystem = MIN_IF_GR;     // too low, but we enable AGC, which shall correct this
	}
	int gRdb = 0;
	int lna_state = -1;

	// Find the correct LNA state setting using LNA Gr table
	// Start from lowest LNA Gr
	for (int i = 0; i < num_lnaGRs[hw_type]; i++) {
		// Can requested gain reduction be reached with this LNA Gr?
		if((gRdBsystem >= lnaGRtables[hw_type][i] + MIN_IF_GR) && (gRdBsystem <= lnaGRtables[hw_type][i] + MAX_IF_GR)) {
			gRdb = gRdBsystem - lnaGRtables[hw_type][i];
			lna_state = i;
			fprintf(stderr, "Selected IF gain reduction: %d dB, LNA gain reduction: %d dB\n",
					gRdb, lnaGRtables[hw_type][i]);
			break;
		}
	}
	// Bail out on impossible gain reduction setting
	if(lna_state < 0) {
		int min_gr = MIN_IF_GR + lnaGRtables[hw_type][0];
		int max_gr = MAX_IF_GR + lnaGRtables[hw_type][num_lnaGRs[hw_type]-1];
		if(hw_type == HW_RSP1A) {
			max_gr += MIXER_GR;     // other RSP types have mixer GR included in the highest LNA state
		}
		fprintf(stderr, "Gain reduction value is out of range (min=%d max=%d)\n", min_gr, max_gr);
		_exit(1);
	}

	SDRPlay.data_index = 0;
	int sdrplaySamplesPerPacket = 0;

	err = mir_sdr_StreamInit (&gRdb, (double)SDRPLAY_RATE/1e6, (double)freq/1e6, mir_sdr_BW_1_536, mir_sdr_IF_Zero,
			lna_state, &gRdBsystem, mir_sdr_USE_RSP_SET_GR, &sdrplaySamplesPerPacket,
			sdrplay_streamCallback, sdrplay_gainCallback, &SDRPlay);
	if(err != mir_sdr_Success) {
		fprintf(stderr, "Unable to initialize RSP stream, error %d\n", err);
		_exit(1);
	}
	initialized = 1;
	debug_print(D_SDR, "Stream initialized (sdrplaySamplesPerPacket=%d gRdB=%d gRdBsystem=%d)\n",
			sdrplaySamplesPerPacket, gRdb, gRdBsystem);

	// If no GR has been specified, enable AGC with a default set point (unless configured otherwise)
	if(gr == SDR_AUTO_GAIN && enable_agc == 0) {
		enable_agc = DEFAULT_AGC_SETPOINT;
	}

	if(enable_agc != 0) {
		err = mir_sdr_AgcControl(mir_sdr_AGC_5HZ, enable_agc, 0, 0, 0, 0, 0);
		if(err != mir_sdr_Success) {
			fprintf(stderr, "Unable to activate AGC, error %d\n", err);
			_exit(1);
		}
		fprintf(stderr, "AGC activated with set point at %d dBFS\n", enable_agc);
	} else {
		err = mir_sdr_AgcControl(mir_sdr_AGC_DISABLE, DEFAULT_AGC_SETPOINT, 0, 0, 0, 0, 0);
		if(err != mir_sdr_Success) {
			fprintf(stderr, "Unable to deactivate AGC, error %d\n", err);
			_exit(1);
		}
	}

	if(mir_sdr_SetDcMode(4, 0) != mir_sdr_Success || mir_sdr_SetDcTrackTime(63) != mir_sdr_Success) {
		fprintf(stderr, "Set DC tracking failed, %d\n", err);
		_exit(1);
	}

	fprintf(stderr, "Device #%d started\n", devIdx);
	while(!do_exit) {
		usleep(1000000);
	}
}

void sdrplay_cancel() {
	if(initialized) {
		mir_sdr_StreamUninit();
		mir_sdr_ReleaseDeviceIdx();
	}
}
