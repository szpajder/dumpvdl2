/*
 *  This file is a part of dumpvdl2
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
#include <stdio.h>              // fprintf
#include <stdlib.h>             // calloc, strtol
#include <string.h>             // strcmp
#include <unistd.h>             // _exit, usleep
#include <sdrplay_api.h>
#include "dumpvdl2.h"           // sbuf, Config
#include "sdrplay3.h"            // SDRPLAY3_OVERSAMPLE

#define SDRPLAY3_MIXER_GR                   19
#define SDRPLAY3_ASYNC_BUF_NUMBER           15
#define SDRPLAY3_ASYNC_BUF_SIZE             (32*16384) // 512k shorts
#define SDRPLAY3_RATE (SYMBOL_RATE * SPS * SDRPLAY3_OVERSAMPLE)

typedef struct {
	void *context;
	HANDLE *dev;
	unsigned char *sdrplay3_data;
	int data_index;
} sdrplay3_ctx_t;

typedef enum {
	HW_UNKNOWN      = 0,
	HW_RSP1         = 1,
	HW_RSP2         = 2,
	HW_RSP1A        = 3,
	HW_RSPDUO       = 4
// TODO: RSPdx
} sdrplay3_hw_type;
#define NUM_HW_TYPES 5

static int initialized = 0;

#define NUM_LNA_STATES 10       // Max number of LNA states of all hw types
static int lnaGRtables[NUM_HW_TYPES][NUM_LNA_STATES] = {
	[HW_RSP1]   = { 0, 24, 19, 43, 0, 0, 0, 0, 0, 0 },
	[HW_RSP2]   = { 0, 10, 15, 21, 24, 34, 39, 45, 64, 0 },
	[HW_RSP1A]  = { 0, 6, 12, 18, 20, 26, 32, 38, 57, 62 },
	[HW_RSPDUO] = { 0, 6, 12, 18, 20, 26, 32, 38, 57, 62 }
// TODO: RSPdx
};
static int num_lnaGRs[NUM_HW_TYPES] = {
	[HW_RSP1] = 4,
	[HW_RSP2] = 9,
	[HW_RSP1A] = 10,
	[HW_RSPDUO] = 10
// TODO: RSPdx
};
static char *hw_descr[NUM_HW_TYPES] = {
	[HW_RSP1] = "RSP1",
	[HW_RSP2] = "RSP2",
	[HW_RSP1A] = "RSP1A",
	[HW_RSPDUO] = "RSPduo"
// TODO: RSPdx
};

static void sdrplay3_streamCallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
		unsigned int numSamples, unsigned int reset, void *cbContext) {
	UNUSED(params);
	UNUSED(reset);
	int i, j, count1, count2, new_buf_flag;
	int end, input_index;
	CAST_PTR(SDRPlay, sdrplay3_ctx_t *, cbContext);
	if(numSamples == 0) {
		return;
	}
	unsigned char *dptr = SDRPlay->sdrplay3_data;
	// data_index counts samples
	// numSamples counts I/Q sample pairs
	end = SDRPlay->data_index + (numSamples * 2);
	count2 = end - (SDRPLAY3_ASYNC_BUF_SIZE * SDRPLAY3_ASYNC_BUF_NUMBER);
	if(count2 < 0) count2 = 0;              // count2 is samples wrapping around to start of buf
	count1 = (numSamples * 2) - count2;     // count1 is samples fitting before the end of buf

	// flag is set if this packet takes us past a multiple of SDRPLAY3_ASYNC_BUF_SIZE
	new_buf_flag = (SDRPlay->data_index / SDRPLAY3_ASYNC_BUF_SIZE == end / SDRPLAY3_ASYNC_BUF_SIZE ? 0 : 1);

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

	if(SDRPlay->data_index >= SDRPLAY3_ASYNC_BUF_SIZE * SDRPLAY3_ASYNC_BUF_NUMBER) {
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

	// send SDRPLAY3_ASYNC_BUF_SIZE samples downstream, if available
	if(new_buf_flag) {
		// go back by one buffer length, then round down further to start of buffer
		end = SDRPlay->data_index - SDRPLAY3_ASYNC_BUF_SIZE;
		if(end < 0) end += SDRPLAY3_ASYNC_BUF_SIZE * SDRPLAY3_ASYNC_BUF_NUMBER;
		end -= end % SDRPLAY3_ASYNC_BUF_SIZE;
		process_buf_short(&SDRPlay->sdrplay3_data[end * sizeof(short)], SDRPLAY3_ASYNC_BUF_SIZE * sizeof(short), NULL);
	}
}

static void sdrplay3_eventCallback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner,
			sdrplay_api_EventParamsT *params, void *cbContext) {
	switch(eventId) {
		case sdrplay_api_GainChange:
			debug_print(D_SDR, "sdrplay_api_EventCb: %s, tuner=%s gRdB=%d lnaGRdB=%d systemGain=%.2f\n",
					"sdrplay_api_GainChange", (tuner == sdrplay_api_Tuner_A)? "sdrplay_api_Tuner_A":
					"sdrplay_api_Tuner_B", params->gainParams.gRdB, params->gainParams.lnaGRdB,
					params->gainParams.currGain);
			break;
		case sdrplay_api_PowerOverloadChange:
			debug_print(D_SDR, "sdrplay_api_PowerOverloadChange: tuner=%s powerOverloadChangeType=%s\n",
					(tuner == sdrplay_api_Tuner_A)? "sdrplay_api_Tuner_A": "sdrplay_api_Tuner_B",
					(params->powerOverloadParams.powerOverloadChangeType ==
					 sdrplay_api_Overload_Detected)? "sdrplay_api_Overload_Detected":
					"sdrplay_api_Overload_Corrected");
			// Send update message to acknowledge power overload message received
			CAST_PTR(ctx, sdrplay3_ctx_t *, cbContext);
			sdrplay_api_Update(ctx->dev, tuner, sdrplay_api_Update_Ctrl_OverloadMsgAck,
					sdrplay_api_Update_Ext1_None);
			break;
		case sdrplay_api_RspDuoModeChange:
			debug_print(D_SDR, "sdrplay_api_EventCb: %s, tuner=%s modeChangeType=%s\n",
					"sdrplay_api_RspDuoModeChange", (tuner == sdrplay_api_Tuner_A)?
					"sdrplay_api_Tuner_A": "sdrplay_api_Tuner_B",
					(params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterInitialised)?
					"sdrplay_api_MasterInitialised":
					(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveAttached)?
					"sdrplay_api_SlaveAttached":
					(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveDetached)?
					"sdrplay_api_SlaveDetached":
					(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveInitialised)?
					"sdrplay_api_SlaveInitialised":
					(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveUninitialised)?
					"sdrplay_api_SlaveUninitialised":
					(params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterDllDisappeared)?
					"sdrplay_api_MasterDllDisappeared":
					(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveDllDisappeared)?
					"sdrplay_api_SlaveDllDisappeared": "unknown type");
			break;
		case sdrplay_api_DeviceRemoved:
			debug_print(D_SDR, "sdrplay_api_EventCb: %s\n", "sdrplay_api_DeviceRemoved");
			break;
		default:
			debug_print(D_SDR, "sdrplay_api_EventCb: %d, unknown event\n", eventId);
			break;
	}
}


static int sdrplay3_verbose_device_search(char *dev, sdrplay_api_DeviceT const *devices,
		uint32_t const dev_cnt, sdrplay3_hw_type * const hw_type) {
	*hw_type = HW_UNKNOWN;
	int devIdx = -1;
	if(dev == NULL) {
		return -1;
	}
	fprintf(stderr, "\nFound %d device(s):\n", dev_cnt);
	for(unsigned int i = 0; i < dev_cnt; i++) {
		fprintf(stderr, "  %u:  SN: %s\n", i, devices[i].SerNo);
	}
	fprintf(stderr, "\n");

	// Does the string match a serial number?
	for (unsigned int i = 0; i < dev_cnt; i++) {
		if(strcmp(dev, devices[i].SerNo) != 0) {
			continue;
		}
		devIdx = i;
		goto dev_found;
	}

	// Does the string look like a raw ID number?
	char *endptr = dev;
	long num = strtol(dev, &endptr, 0);
	if(endptr[0] == '\0' && num >= 0 && num < dev_cnt) {
		devIdx = (unsigned int)num;
		goto dev_found;
	}

	fprintf(stderr, "No matching devices found\n");
	return -1;

dev_found:
	if(devices[devIdx].hwVer == 1) {
		*hw_type = HW_RSP1;
	} else if(devices[devIdx].hwVer == 2) {
		*hw_type = HW_RSP2;
	} else if(devices[devIdx].hwVer == 3) {
		*hw_type = HW_RSPDUO;
	} else if(devices[devIdx].hwVer > 253) {
		*hw_type = HW_RSP1A;
// TODO: RSPdx
	} else {
		fprintf(stderr, "Selected device #%d is unsupported: hardware version %d\n",
				devIdx, devices[devIdx].hwVer);
		return -1;
	}

	fprintf(stderr, "Selected device #%d (type: %s SN: %s)\n",
			devIdx, hw_descr[*hw_type], devices[devIdx].SerNo);
	return devIdx;
}

void sdrplay3_init(vdl2_state_t * const ctx, char * const dev, char * const antenna,
		double const freq, int const gr, double const freq_correction_ppm, int const enable_biast,
		int const enable_notch_filter, int agc_set_point, int tuner) {
	UNUSED(ctx);

	sdrplay_api_ErrT err;
	float ver = 1.0f;
	sdrplay3_ctx_t SDRPlay;
	sdrplay3_hw_type hw_type = HW_UNKNOWN;

	err = sdrplay_api_Open();
	if (err != sdrplay_api_Success) {
		fprintf(stderr, "sdrplay_api_Open failed: %s\n", sdrplay_api_GetErrorString(err));
		_exit(1);
	}

	err = sdrplay_api_ApiVersion(&ver);
	if(err != sdrplay_api_Success) {
		fprintf(stderr, "sdrplay_api_ApiVersion failed: %s\n", sdrplay_api_GetErrorString(err));
		_exit(1);
	}
	if(ver != SDRPLAY_API_VERSION) {
		fprintf(stderr, "SDRplay library version %f does not match "
				"version the program has been compiled with (%f)\n", ver, SDRPLAY_API_VERSION);
		_exit(1);
	}
	fprintf(stderr, "Using SDRPlay API version %f\n", ver);

#ifdef DEBUG
	if(Config.debug_filter & D_SDR) {
		err = sdrplay_api_DebugEnable(NULL, 1);
		if (err != sdrplay_api_Success) {
			fprintf(stderr, "sdrplay_api_DebugEnable failed: %s\n", sdrplay_api_GetErrorString(err));
			goto fail;
		}
	}
#endif

	sdrplay_api_LockDeviceApi();
	sdrplay_api_DeviceT devices[SDRPLAY_MAX_DEVICES];
	uint32_t dev_cnt;

	err = sdrplay_api_GetDevices(devices, &dev_cnt, SDRPLAY_MAX_DEVICES);
	if(err != sdrplay_api_Success) {
		fprintf(stderr, "Unable to enumerate connected SDRPlay devices; %s\n", sdrplay_api_GetErrorString(err));
		goto unlock_and_fail;
	}
	if(dev_cnt < 1) {
		fprintf(stderr, "No SDRplay devices found\n");
		goto unlock_and_fail;
	}

	int dev_idx = sdrplay3_verbose_device_search(dev, devices, dev_cnt, &hw_type);
	if(dev_idx < 0) {
		goto unlock_and_fail;
	}
	sdrplay_api_DeviceT *device = devices + dev_idx;

	err = sdrplay_api_SelectDevice(device);
	if(err != sdrplay_api_Success) {
		fprintf(stderr, "Unable to select device %s: %s\n", device->SerNo, sdrplay_api_GetErrorString(err));
		goto unlock_and_fail;
	}
	sdrplay_api_UnlockDeviceApi();

#ifdef DEBUG
	if(Config.debug_filter & D_SDR) {
		err = sdrplay_api_DebugEnable(device, 1);
		if (err != sdrplay_api_Success) {
			fprintf(stderr, "sdrplay_api_DebugEnable failed: %s\n", sdrplay_api_GetErrorString(err));
			goto fail;
		}
	}
#endif

	sdrplay_api_DeviceParamsT *devParams = NULL;
	err = sdrplay_api_GetDeviceParams(device->dev, &devParams);
	if(err != sdrplay_api_Success || devParams == NULL) {
		fprintf(stderr, "Unable to read device %s parameters: %s\n", device->SerNo, sdrplay_api_GetErrorString(err));
		goto fail;
	}
	devParams->devParams->fsFreq.fsHz = SDRPLAY3_RATE;
	devParams->devParams->ppm = freq_correction_ppm;

	sdrplay_api_RxChannelParamsT *chParams = devParams->rxChannelA;
	// FIXME: bandwidth should be configurable and the default should be lower
	// as VDL2 channels often fit within 0.5 MHz of spectrum.
	chParams->tunerParams.bwType = sdrplay_api_BW_1_536;
	chParams->tunerParams.ifType = sdrplay_api_IF_Zero;
	chParams->tunerParams.rfFreq.rfHz = freq;

	if(hw_type == HW_RSP2) {
		if(enable_biast) {
			chParams->rsp2TunerParams.biasTEnable = 1;
			fprintf(stderr, "RSP2: Enabling Bias-T\n");
		}
		if(strcmp(antenna, "A") == 0) {
			chParams->rsp2TunerParams.antennaSel = sdrplay_api_Rsp2_ANTENNA_A;
		} else if(strcmp(antenna, "B") == 0) {
			chParams->rsp2TunerParams.antennaSel = sdrplay_api_Rsp2_ANTENNA_B;
		} else {
			fprintf(stderr, "RSP2: Invalid antenna port specified\n");
			goto fail;
		}
		fprintf(stderr, "RSP2: Selecting antenna port %s\n", antenna);

		if(enable_notch_filter) {
			chParams->rsp2TunerParams.rfNotchEnable = 1;
			fprintf(stderr, "RSP2: Enabling notch filter\n");
		}
	} else if(hw_type == HW_RSP1A) {
		if(enable_biast) {
			chParams->rsp1aTunerParams.biasTEnable = 1;
			fprintf(stderr, "RSP1A: Enabling Bias-T\n");
		}
		if(enable_notch_filter) {
			devParams->devParams->rsp1aParams.rfNotchEnable = 1;
			fprintf(stderr, "RSP1A: Enabling notch filter\n");
		}
		// TODO: DAB notch support
	} else if(hw_type == HW_RSPDUO) {
		if(tuner != 1) {
			// FIXME: add tuner 2 support
			fprintf(stderr, "RSPduo: tuner %d not supported\n", tuner);
			goto fail;
		}
		fprintf(stderr, "RSPduo: Selecting tuner %d\n", tuner);
		if(enable_biast) {
			chParams->rspDuoTunerParams.biasTEnable = 1;
			fprintf(stderr, "RSPduo: Enabling Bias-T\n");
		}
		if(enable_notch_filter) {
			chParams->rsp2TunerParams.rfNotchEnable = 1;
			fprintf(stderr, "RSPduo: Enabling notch filter\n");
		}
		// TODO: DAB notch support
	}

	int gRdBsystem = gr;    // FIXME: no longer needed
	if(gr == SDR_AUTO_GAIN) {
		gRdBsystem = sdrplay_api_NORMAL_MIN_GR;     // too low, but we enable AGC, which shall correct this
		if(agc_set_point != 0) {
			chParams->ctrlParams.agc.setPoint_dBfs = agc_set_point;
		}
		chParams->ctrlParams.agc.enable = sdrplay_api_AGC_5HZ;
		fprintf(stderr, "Enabling AGC with set point at %d dBFS\n", chParams->ctrlParams.agc.setPoint_dBfs);
	} else {    // AGC disabled, gain reduction configured manually
		int gRdB = 0;
		int lna_state = -1;

		// Find the correct LNA state setting using LNA Gr table
		// Start from lowest LNA Gr
		for (int i = 0; i < num_lnaGRs[hw_type]; i++) {
			// Can requested gain reduction be reached with this LNA Gr?
			if((gRdBsystem >= lnaGRtables[hw_type][i] + sdrplay_api_NORMAL_MIN_GR) && (gRdBsystem <= lnaGRtables[hw_type][i] + MAX_BB_GR)) {
				gRdB = gRdBsystem - lnaGRtables[hw_type][i];
				lna_state = i;
				fprintf(stderr, "Selected IF gain reduction: %d dB, RF gain reduction: %d dB\n",
						gRdB, lnaGRtables[hw_type][i]);
				break;
			}
		}
		// Bail out on impossible gain reduction setting
		if(lna_state < 0) {
			int min_gr = sdrplay_api_NORMAL_MIN_GR + lnaGRtables[hw_type][0];
			int max_gr = MAX_BB_GR + lnaGRtables[hw_type][num_lnaGRs[hw_type]-1];
			if(hw_type == HW_RSP1A) {
				max_gr += SDRPLAY3_MIXER_GR;     // other RSP types have mixer GR included in the highest LNA state
			}
			fprintf(stderr, "Gain reduction value is out of range (min=%d max=%d)\n", min_gr, max_gr);
			goto fail;
		}
		fprintf(stderr, "Disabling AGC\n");
		chParams->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
		fprintf(stderr, "Setting gain: GR=%d LNAState=%d\n", gRdB, lna_state);
		chParams->tunerParams.gain.gRdB = gRdB;
		chParams->tunerParams.gain.LNAstate = lna_state;
	}

	sdrplay_api_CallbackFnsT callbacks;
	callbacks.StreamACbFn = sdrplay3_streamCallback;
	callbacks.StreamBCbFn = NULL;
	callbacks.EventCbFn = sdrplay3_eventCallback;

	SDRPlay.sdrplay3_data = XCALLOC(SDRPLAY3_ASYNC_BUF_SIZE * SDRPLAY3_ASYNC_BUF_NUMBER, sizeof(short));
	SDRPlay.data_index = 0;
	SDRPlay.dev = device->dev;
	sbuf = XCALLOC(SDRPLAY3_ASYNC_BUF_SIZE, sizeof(float));

	err = sdrplay_api_Init(device->dev, &callbacks, &SDRPlay);
	if(err != sdrplay_api_Success) {
		fprintf(stderr, "SDRplay: device initialization failed: %s\n", sdrplay_api_GetErrorString(err));
		goto fail;
	}

	if(err != sdrplay_api_Success) {
		fprintf(stderr, "Unable to initialize sample stream: %s\n", sdrplay_api_GetErrorString(err));
		goto fail;
	}
	initialized = 1;
	debug_print(D_SDR, "Stream initialized (sdrplaySamplesPerPacket=%d)\n",
			devParams->devParams->samplesPerPkt);

	fprintf(stderr, "Device %s started\n", device->SerNo);
	while(!do_exit) {
		usleep(1000000);
	}
	fprintf(stderr, "SDRplay: stopping device\n");
	err = sdrplay_api_Uninit(device->dev);
	if(err != sdrplay_api_Success) {
		fprintf(stderr, "Could not uninitialize SDRplay API: %s\n", sdrplay_api_GetErrorString(err));
	}
	err = sdrplay_api_ReleaseDevice(device);
	if(err != sdrplay_api_Success) {
		fprintf(stderr, "Could not release SDRplay device: %s\n", sdrplay_api_GetErrorString(err));
	}
	sdrplay_api_Close();
	return;

unlock_and_fail:
	sdrplay_api_UnlockDeviceApi();

fail:
	sdrplay_api_Close();
	_exit(1);
}

void sdrplay3_cancel() {
//	if(initialized) {
//	}
}
