/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2023 Tomasz Lemiech <szpajder@gmail.com>
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
#include <libacars/dict.h>      // la_dict
#include "dumpvdl2.h"           // sbuf, Config
#include "sdrplay3.h"           // SDRPLAY3_OVERSAMPLE

#define SDRPLAY3_ASYNC_BUF_NUMBER           15
#define SDRPLAY3_ASYNC_BUF_SIZE             (32*16384) // 512k shorts
#define SDRPLAY3_DEFAULT_AGC_SETPOINT       -30

typedef struct {
	void *context;
	HANDLE *dev;
	unsigned char *sdrplay3_data;
	int data_index;
} sdrplay3_ctx_t;

static char const *get_hw_descr(int hw_id) {
	static la_dict const hw_descr[] = {
		{ .id = SDRPLAY_RSP1_ID, .val = "RSP1" },
		{ .id = SDRPLAY_RSP2_ID, .val = "RSP2" },
		{ .id = SDRPLAY_RSP1A_ID, .val = "RSP1A" },
		{ .id = SDRPLAY_RSPduo_ID, .val = "RSPduo" },
		{ .id = SDRPLAY_RSPdx_ID, .val = "RSPdx" },
		{ .id = 0, .val = NULL }
	};
	char const *ret = la_dict_search(hw_descr, hw_id);
	return ret ? ret : "<unknown>";
};

static void sdrplay3_streamCallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
		unsigned int numSamples, unsigned int reset, void *cbContext) {
	UNUSED(params);
	UNUSED(reset);
	int i, j, count1, count2, new_buf_flag;
	int end, input_index;
	sdrplay3_ctx_t *SDRPlay = cbContext;
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
#ifndef DEBUG
	UNUSED(params);
#endif
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
			sdrplay3_ctx_t *ctx = cbContext;
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

static void sdrplay3_set_biast(sdrplay_api_DeviceParamsT *devParams,
		sdrplay_api_RxChannelParamsT * chParams, uint8_t hwVer) {
	switch(hwVer) {
		case SDRPLAY_RSP1_ID:
			fprintf(stderr, "%s: Not enabling Bias-T: feature not supported\n",
					get_hw_descr(hwVer));
			return;
		case SDRPLAY_RSP2_ID:
			chParams->rsp2TunerParams.biasTEnable = 1;
			break;
		case SDRPLAY_RSP1A_ID:
			chParams->rsp1aTunerParams.biasTEnable = 1;
			break;
		case SDRPLAY_RSPduo_ID:
			chParams->rspDuoTunerParams.biasTEnable = 1;
			break;
		case SDRPLAY_RSPdx_ID:
			devParams->devParams->rspDxParams.biasTEnable = 1;
			break;
		default:
			fprintf(stderr, "Not enabling Bias-T: unknown device type %u\n", hwVer);
			return;
	}
	fprintf(stderr, "%s: Enabling Bias-T\n", get_hw_descr(hwVer));
}

static void sdrplay3_set_notch_filter(sdrplay_api_DeviceParamsT *devParams,
		sdrplay_api_RxChannelParamsT *chParams, uint8_t hwVer) {
	switch(hwVer) {
		case SDRPLAY_RSP1_ID:
			fprintf(stderr, "%s: Not enabling notch filter: feature not supported\n",
					get_hw_descr(hwVer));
			return;
		case SDRPLAY_RSP2_ID:
			chParams->rsp2TunerParams.rfNotchEnable = 1;
			break;
		case SDRPLAY_RSP1A_ID:
			devParams->devParams->rsp1aParams.rfNotchEnable = 1;
			break;
		case SDRPLAY_RSPduo_ID:
			chParams->rspDuoTunerParams.rfNotchEnable = 1;
			break;
		case SDRPLAY_RSPdx_ID:
			devParams->devParams->rspDxParams.rfNotchEnable = 1;
			break;
		default:
			fprintf(stderr, "Not enabling notch filter: unknown device type %u\n", hwVer);
			return;
	}
	fprintf(stderr, "%s: Enabling notch filter\n", get_hw_descr(hwVer));
}

static void sdrplay3_set_dab_notch_filter(sdrplay_api_DeviceParamsT *devParams,
		sdrplay_api_RxChannelParamsT *chParams, uint8_t hwVer) {
	switch(hwVer) {
		case SDRPLAY_RSP1_ID:
		case SDRPLAY_RSP2_ID:
			fprintf(stderr, "%s: Not enabling DAB notch filter: feature not supported\n",
					get_hw_descr(hwVer));
			return;
		case SDRPLAY_RSP1A_ID:
			devParams->devParams->rsp1aParams.rfDabNotchEnable = 1;
			break;
		case SDRPLAY_RSPduo_ID:
			chParams->rspDuoTunerParams.rfDabNotchEnable = 1;
			break;
		case SDRPLAY_RSPdx_ID:
			devParams->devParams->rspDxParams.rfDabNotchEnable = 1;
			break;
		default:
			fprintf(stderr, "Not enabling DAB notch filter: unknown device type %u\n", hwVer);
			return;
	}
	fprintf(stderr, "%s: Enabling DAB notch filter\n", get_hw_descr(hwVer));
}

static void sdrplay3_select_antenna(sdrplay_api_DeviceParamsT *devParams,
		sdrplay_api_RxChannelParamsT *chParams, uint8_t hwVer, char const *antenna) {
	UNUSED(devParams);
	switch(hwVer) {
		case SDRPLAY_RSP2_ID:
			if(strcmp(antenna, "A") == 0) {
				chParams->rsp2TunerParams.antennaSel = sdrplay_api_Rsp2_ANTENNA_A;
			} else if(strcmp(antenna, "B") == 0) {
				chParams->rsp2TunerParams.antennaSel = sdrplay_api_Rsp2_ANTENNA_B;
			} else {
				fprintf(stderr, "%s: Invalid antenna port specified\n", get_hw_descr(hwVer));
				return;
			}
			break;
		case SDRPLAY_RSP1_ID:
		case SDRPLAY_RSP1A_ID:
		case SDRPLAY_RSPduo_ID:
			fprintf(stderr, "%s: Cannot select antenna port: feature not supported\n",
					get_hw_descr(hwVer));
			return;
		case SDRPLAY_RSPdx_ID:
			if(strcmp(antenna, "A") == 0) {
				devParams->devParams->rspDxParams.antennaSel = sdrplay_api_RspDx_ANTENNA_A;
			} else if(strcmp(antenna, "B") == 0) {
				devParams->devParams->rspDxParams.antennaSel = sdrplay_api_RspDx_ANTENNA_B;
			} else if(strcmp(antenna, "C") == 0) {
				devParams->devParams->rspDxParams.antennaSel = sdrplay_api_RspDx_ANTENNA_C;
			} else {
				fprintf(stderr, "%s: Invalid antenna port specified\n", get_hw_descr(hwVer));
				return;
			}
			break;
		default:
			fprintf(stderr, "Cannot select antenna port: unknown device type %u\n", hwVer);
			return;
	}
	fprintf(stderr, "%s: Selecting antenna port %s\n", get_hw_descr(hwVer), antenna);
}

static int sdrplay3_verbose_device_search(char const *dev, sdrplay_api_DeviceT const *devices,
		uint32_t dev_cnt) {
	int devIdx = -1;
	if(dev == NULL) {
		return -1;
	}
	fprintf(stderr, "\nFound %d device(s):\n", dev_cnt);
	for(uint32_t i = 0; i < dev_cnt; i++) {
		fprintf(stderr, "  %u: Type: %s SN: %s\n", i, get_hw_descr(devices[i].hwVer), devices[i].SerNo);
	}
	fprintf(stderr, "\n");

	// Does the string match a serial number?
	for (uint32_t i = 0; i < dev_cnt; i++) {
		if(strcmp(dev, devices[i].SerNo) != 0) {
			continue;
		}
		devIdx = i;
		goto dev_found;
	}

	// Does the string look like a raw ID number?
	char *endptr = (char *)dev;
	long num = strtol(dev, &endptr, 0);
	if(endptr[0] == '\0' && num >= 0 && (uint32_t)num < dev_cnt) {
		devIdx = (int)num;
		goto dev_found;
	}

	fprintf(stderr, "No matching devices found\n");
	return -1;

dev_found:
	fprintf(stderr, "Selected device #%d (type: %s SN: %s)\n",
			devIdx, get_hw_descr(devices[devIdx].hwVer), devices[devIdx].SerNo);
	return devIdx;
}

void sdrplay3_init(vdl2_state_t const *ctx, char const *dev, uint32_t sample_rate, char const *antenna,
		double freq, int ifgr, int lna_state, double freq_correction_ppm,
		int enable_biast, int enable_notch_filter, int enable_dab_notch_filter,
		int agc_set_point, int tuner) {
	UNUSED(ctx);

	sdrplay_api_ErrT err;
	float ver = 1.0f;
	sdrplay3_ctx_t SDRPlay;

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

	int dev_idx = sdrplay3_verbose_device_search(dev, devices, dev_cnt);
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
	devParams->devParams->fsFreq.fsHz = sample_rate;
	devParams->devParams->ppm = freq_correction_ppm;

	sdrplay_api_RxChannelParamsT *chParams = devParams->rxChannelA;
	chParams->tunerParams.bwType = sdrplay_api_BW_1_536;
	chParams->tunerParams.ifType = sdrplay_api_IF_Zero;
	chParams->tunerParams.rfFreq.rfHz = freq;

	if(enable_biast) {
		sdrplay3_set_biast(devParams, chParams, device->hwVer);
	}
	if(enable_notch_filter) {
		sdrplay3_set_notch_filter(devParams, chParams, device->hwVer);
	}
	if(enable_dab_notch_filter) {
		sdrplay3_set_dab_notch_filter(devParams, chParams, device->hwVer);
	}
	if(antenna != NULL) {
		sdrplay3_select_antenna(devParams, chParams, device->hwVer, antenna);
	}
	if(device->hwVer == SDRPLAY_RSPduo_ID) {
		debug_print(D_SDR, "RSPduo: available modes: 0x%x\n", device->rspDuoMode);
		if((device->rspDuoMode & sdrplay_api_RspDuoMode_Master) == 0) {
			fprintf(stderr, "%s: Master device not available\n", get_hw_descr(device->hwVer));
			fprintf(stderr, "This device can only be used in single tuner mode\n");
			goto fail;
		}
		device->rspDuoMode = sdrplay_api_RspDuoMode_Single_Tuner;
		switch(tuner) {
			case 1:
				device->tuner = sdrplay_api_Tuner_A;
				break;
			case 2:
				device->tuner = sdrplay_api_Tuner_B;
				break;
			default:
				fprintf(stderr, "%s: Invalid tuner specified\n", get_hw_descr(device->hwVer));
				return;
		}
		fprintf(stderr, "%s: Using tuner %d\n", get_hw_descr(device->hwVer), tuner);
	}

	if(ifgr < 0 || lna_state < 0) {
		chParams->ctrlParams.agc.setPoint_dBfs = agc_set_point < 0 ? agc_set_point : SDRPLAY3_DEFAULT_AGC_SETPOINT;
		chParams->ctrlParams.agc.enable = sdrplay_api_AGC_5HZ;
		fprintf(stderr, "Enabling AGC with set point at %d dBFS\n", chParams->ctrlParams.agc.setPoint_dBfs);
	} else {    // AGC disabled, IFGR and LNAstate configured manually
		fprintf(stderr, "Disabling AGC\n");
		chParams->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
		fprintf(stderr, "Setting gain reduction components: IFGR=%d LNAState=%d\n", ifgr, lna_state);
		chParams->tunerParams.gain.gRdB = ifgr;
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
	// NO-OP - sdrplay3_init will release the device after do_exit flag is raised.
}
