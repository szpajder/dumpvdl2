/*
 *  dumpvdl2 - a VDL Mode 2 message decoder and protocol analyzer
 *
 *  Copyright (c) 2017 Fabrice Crohas <fcrohas@gmail.com>
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
#include <string.h>
#include <unistd.h>
#include "sdrplay.h"
#include "mirsdrapi-rsp.h"

static int initialized = 0;

static int lnaGRtables[NUM_HW_TYPES][NUM_LNA_STATES] = {
	[HW_RSP1]  = { 0, 24, 19, 43, 0, 0, 0, 0, 0, 0 },
	[HW_RSP2]  = { 0, 10, 15, 21, 24, 34, 39, 45, 64, 0 },
	[HW_RSP1A] = { 0, 6, 12, 18, 20, 26, 32, 38, 57, 62 }
};
static int num_lnaGRs[NUM_HW_TYPES] = {
	[HW_RSP1] = 4,
	[HW_RSP2] = 9,
	[HW_RSP1A] = 10
};
static char *hw_descr[NUM_HW_TYPES] = {
	[HW_RSP1] = "RSP1",
	[HW_RSP2] = "RSP2",
	[HW_RSP1A] = "RSP1A"
};

void sdrplay_init(vdl2_state_t *ctx, char *dev, char *antenna, uint32_t freq, float gain,
int ppm_error, int enable_biast, int enable_notch_filter, int enable_agc) {
	mir_sdr_ErrT err;
	float ver;
	struct sdrplay_t SDRPlay;
	mir_sdr_DeviceT devices[4];
	sdrplay_hw_type hw_type = HW_UNKNOWN;
	unsigned int numDevs;
	int devAvail = 0;
	int device = atoi(dev);
	SDRPlay.context = ctx;
	SDRPlay.lna_state = 0;
	SDRPlay.autogain = 0;
	/* Check API version */
	err = mir_sdr_ApiVersion(&ver);
	if ((err!= mir_sdr_Success) || (ver != MIR_SDR_API_VERSION)) {
		fprintf(stderr, "Incorrect API version %f\n", ver);
		_exit(1);
	}
#if DEBUG
	mir_sdr_DebugEnable(1);
#endif
	err = mir_sdr_GetDevices(&devices[0], &numDevs, 4);
	if (err!= mir_sdr_Success) {
		fprintf(stderr, "Unable to get connected devices, error %d\n", err);
		_exit(1);
	}
	// Check how much devices are available
	for(int i = 0; i < numDevs; i++) {
		if(devices[i].devAvail == 1) {
			devAvail++;
		}
	}
	// No device
	if (devAvail == 0) {
		fprintf(stderr, "ERROR: No RSP devices available.\n");
		_exit(1);
	}
	// Check if selected device is available
	if (devices[device].devAvail != 1) {
		fprintf(stderr, "ERROR: RSP selected #%d is not available.\n", device);
		_exit(1);
	}
	if(devices[device].hwVer == 1) {
		hw_type = HW_RSP1;
	} else if(devices[device].hwVer == 2) {
		hw_type = HW_RSP2;
	} else if(devices[device].hwVer > 253) {
		hw_type = HW_RSP1A;
	} else {
		fprintf(stderr, "Unsupported device: hardware version %d\n", devices[device].hwVer);
		_exit(1);
	}

	// Select device
	err = mir_sdr_SetDeviceIdx(device);
	if (err!= mir_sdr_Success) {
		fprintf(stderr, "Unable to select device #%d, error %d\n", device, err);
		_exit(1);
	}
	fprintf(stderr, "Using SDRPlay %s (serial %s) with API version %.3f\n",
		hw_descr[hw_type],
		(devices[device].SerNo != NULL ? devices[device].SerNo : "unknown"),
		ver);
	// Those options are only available on RSP2
	if (hw_type == HW_RSP2) {
		/* Activate biast */
		if (enable_biast) {
			fprintf(stderr, "Bias-t activated\n");
			err = mir_sdr_RSPII_BiasTControl(1);
			if (err!= mir_sdr_Success) {
				fprintf(stderr, "Unable to activate bias-t, error : %d\n", err);
				_exit(1);
			}
		}
		/* Activate antenna */
		if (strcmp(antenna, "A") == 0) {
			err = mir_sdr_RSPII_AntennaControl(mir_sdr_RSPII_ANTENNA_A);
		} else {
			err = mir_sdr_RSPII_AntennaControl(mir_sdr_RSPII_ANTENNA_B);
		}
		if (err!= mir_sdr_Success) {
			fprintf(stderr, "Unable to select antenna %s, error : %d\n", antenna, err);
			_exit(1);
		}
		fprintf(stderr, "Antenna %s activated\n", antenna);
		// Activate notch filter ?
		if (enable_notch_filter) {
			fprintf(stderr, "Notch AM/FM filter activated\n");
			err = mir_sdr_RSPII_RfNotchEnable(1);
			if (err!= mir_sdr_Success) {
				fprintf(stderr, "Unable to activate notch filter, error : %d\n", err);
				_exit(1);
			}
		}
	}

	/* DC Offset Mode */
	err = mir_sdr_DCoffsetIQimbalanceControl(1, 0);
	if (err!= mir_sdr_Success) {
		fprintf(stderr, "Failed to set DC/IQ correction, error %d\n", err);
		_exit(1);
	}
	/* Frequency correction */
	err = mir_sdr_SetPpm(ppm_error);
	if (err!= mir_sdr_Success) {
		fprintf(stderr, "Unable to set frequency correction, error %d\n", err);
		_exit(1);
	}
	fprintf(stderr, "Frequency correction set to %d ppm\n", ppm_error);

	/* Allocate 16-bit interleaved I and Q buffers */
	SDRPlay.sdrplay_data = XCALLOC(ASYNC_BUF_SIZE * ASYNC_BUF_NUMBER, sizeof(short));
	ctx->sbuf = XCALLOC(ASYNC_BUF_SIZE, sizeof(float));
	int gRdBsystem = 0;
	int gRdb = 0;
	if (gain == MODES_AUTO_GAIN) {
		SDRPlay.autogain = 1;
		SDRPlay.lna_state = 3;	// ?
		gRdb = 38;		// ?
	} else {
		SDRPlay.lna_state = -1;
		// Convert gain to gain reduction
		// FIXME: the constant probably depends on hw_type due to different max gr of LNA
		gRdBsystem = 102.f - gain;
		// Find correct LNA state setting using LNA Gr table
		// Start from lowest LNA Gr
		for (int i = 0; i < num_lnaGRs[hw_type]; i++) {
			// If selected gain reduction can be reach within current lnastate
			if ((gRdBsystem >= lnaGRtables[hw_type][i] + MIN_RSP_GR) && (gRdBsystem <= lnaGRtables[hw_type][i] + MAX_RSP_GR)) {
				gRdb = gRdBsystem - lnaGRtables[hw_type][i];
				SDRPlay.lna_state = i;
				fprintf(stderr, "Selected IF gain reduction: %d dB, LNA gain reduction: %d dB (state=%d)\n",
					gRdb, lnaGRtables[hw_type][i], SDRPlay.lna_state);
				break;
			}
		}
		// Bail out on impossible gain reduction setting
		if(SDRPlay.lna_state < 0) {
			fprintf(stderr, "Unable to set gain: out of range\n");
			_exit(1);
		}
	}
	// Initialize SDRPLAY ACC
	SDRPlay.max_sig = MIN_GAIN_THRESH << ACC_SHIFT;
	SDRPlay.max_sig_acc = MIN_GAIN_THRESH << ACC_SHIFT;
	SDRPlay.data_index = 0;
	// Setup data stream
	debug_print("gainR=%d samp_rate=%f frequency=%f bwKHz=%d ifkHz=%d rspLNA=%d gRdBsystem=%d, grMode=%d, samplesperpacket=%d\n",
		gRdb, SDRPLAY_RATE/1e6, freq/1e6, mir_sdr_BW_1_536, mir_sdr_IF_Zero, SDRPlay.lna_state,
		gRdBsystem, mir_sdr_USE_RSP_SET_GR, SDRPlay.sdrplaySamplesPerPacket);
	err = mir_sdr_StreamInit (&gRdb, (double)SDRPLAY_RATE/1e6, (double)freq/1e6, mir_sdr_BW_1_536, mir_sdr_IF_Zero,
		SDRPlay.lna_state, &gRdBsystem, mir_sdr_USE_RSP_SET_GR, &SDRPlay.sdrplaySamplesPerPacket,
		sdrplay_streamCallback, sdrplay_gainCallback, &SDRPlay);
	if (err != mir_sdr_Success) {
		fprintf(stderr, "Unable to initialize RSP stream, error %d\n", err);
		_exit(1);
	}
	initialized = 1;
	fprintf(stderr, "Stream initialized (sdrplaySamplesPerPacket=%d gRdB=%d gRdBsystem=%d)\n",
		SDRPlay.sdrplaySamplesPerPacket, gRdb, gRdBsystem);

	/* Enable AGC control */
	if (enable_agc != 0) {
		err = mir_sdr_AgcControl(mir_sdr_AGC_5HZ, enable_agc, 0, 0, 0, 0, 0); // 5 Hz?
		if (err!= mir_sdr_Success) {
			fprintf(stderr, "Unable to activate AGC, error %d\n", err);
			_exit(1);
		}
		fprintf(stderr, "AGC activated with set point at %d dBFS\n", enable_agc);
	} else {
		err = mir_sdr_AgcControl(mir_sdr_AGC_DISABLE, -30, 0, 0, 0, 0, 0);
		if (err!= mir_sdr_Success) {
			fprintf(stderr, "Unable to deactivate AGC, error %d\n", err);
			_exit(1);
		}
	}
	/* Configure DC tracking in tuner */
	err = mir_sdr_SetDcMode(4, 0);
	err |= mir_sdr_SetDcTrackTime(63);
	if (err) {
		fprintf(stderr, "Set DC tracking failed, %d\n", err);
		_exit(1);
	}

	fprintf(stderr, "Device #%d started\n", device);
	// Wait for exit
	while(!do_exit) {
		usleep(1000000);
	}
}

void sdrplay_cancel() {
	if(initialized) {
// Deinitialize stream
		mir_sdr_Uninit();
// Release device
		mir_sdr_ReleaseDeviceIdx();
	}
}

void sdrplay_streamCallback(short *xi, short *xq, unsigned int firstSampleNum, int grChanged,
int rfChanged, int fsChanged, unsigned int numSamples, unsigned int reset, void *cbContext) {
	int i, j, count1, count2, new_buf_flag;
	int end, input_index;
	struct sdrplay_t *SDRPlay = (struct sdrplay_t*)cbContext;
	if (numSamples == 0) {
		return;
	}
	unsigned char *dptr = SDRPlay->sdrplay_data;
// data_index is in samples
// numSamples is I/Q sample pairs
	end = SDRPlay->data_index + (numSamples * 2);
	count2 = end - (ASYNC_BUF_SIZE * ASYNC_BUF_NUMBER);
	if (count2 < 0) count2 = 0;			// count2 is samples wrapping around to start of buf
	count1 = (numSamples * 2) - count2;		// count1 is samples fitting before the end of buf

	// flag is set if this packet takes us past a multiple of ASYNC_BUF_SIZE
	new_buf_flag = (SDRPlay->data_index / ASYNC_BUF_SIZE == end / ASYNC_BUF_SIZE ? 0 : 1);

	// now interleave data from I/Q into circular buffer, and note max I value
	input_index = 0;
	SDRPlay->max_sig = 0;

	for (i = 0, j = SDRPlay->data_index * sizeof(short); i < count1 / 2; i++) {
		// Copy I low / high part
		dptr[j++] = xi[input_index] & 0xff;
		dptr[j++] = (xi[input_index] >> 8) & 0xff;
		// Copy Q low / high part
		dptr[j++] = xq[input_index] & 0xff;
		dptr[j++] = (xq[input_index] >> 8 ) & 0xff;
		if (xi[input_index] > SDRPlay->max_sig) SDRPlay->max_sig = xi[input_index];
		input_index++;
	}
	SDRPlay->data_index += count1;

	/* apply slowly decaying filter to max signal value */
	SDRPlay->max_sig -= 16384;
	SDRPlay->max_sig_acc += SDRPlay->max_sig;
	SDRPlay->max_sig = SDRPlay->max_sig_acc >> ACC_SHIFT;
	SDRPlay->max_sig_acc -= SDRPlay->max_sig;

	/* this code is triggered as we reach the end of our circular buffer */
	if (SDRPlay->data_index >= ASYNC_BUF_SIZE * ASYNC_BUF_NUMBER) {
		SDRPlay->data_index = 0;	// pointer back to start of buffer */

		/* adjust gain if automatically */
		if (SDRPlay->autogain) {
			// Measure current gain
			if (SDRPlay->max_sig > MAX_GAIN_THRESH) {
				// If max gain reached for this LNA state
				// Increase LNA state
				if (SDRPlay->gRdB >= MAX_RSP_GR) {
					if (SDRPlay->lna_state < NUM_LNA_STATES - 1) {
						SDRPlay->lna_state += 1;
						// move gain to min as we move on higher lna state attenuation
						// Absolute to force new gain
						mir_sdr_RSP_SetGr(MIN_RSP_GR, SDRPlay->lna_state, 1, 0);
					}
				} else {
					mir_sdr_RSP_SetGr (1, SDRPlay->lna_state, 0, 0);
				}
			}
			if (SDRPlay->max_sig < MIN_GAIN_THRESH) {
				if (SDRPlay->gRdB <= MIN_RSP_GR) {
					// Only for positive LNA state
					if (SDRPlay->lna_state > 0) {
						// Decrease LNA gain
						SDRPlay->lna_state -= 1;
						// move gain to max as we move on lower lna state attenuation
						// Absolute to force new gain
						mir_sdr_RSP_SetGr(MAX_RSP_GR, SDRPlay->lna_state, 1, 0);
					}
				} else {
					mir_sdr_RSP_SetGr (-1, SDRPlay->lna_state, 0, 0);
				}
			}
		}
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
	if (new_buf_flag) {
		/* go back by one buffer length, then round down further to start of buffer */
		end = SDRPlay->data_index - ASYNC_BUF_SIZE;
		if(end < 0) end += ASYNC_BUF_SIZE * ASYNC_BUF_NUMBER;
		end -= end % ASYNC_BUF_SIZE;
		process_buf_short(&SDRPlay->sdrplay_data[end * sizeof(short)], ASYNC_BUF_SIZE * sizeof(short), SDRPlay->context);
	}
}

void sdrplay_gainCallback(unsigned int gRdB, unsigned int lnaGRdB, void *cbContext) {
	struct sdrplay_t *SDRPlay = (struct sdrplay_t*)cbContext;
	SDRPlay->gRdB = gRdB;
	debug_print("Gain callback event gRdb=%d lnaGRdB=%d \n", gRdB, lnaGRdB);
}
