/*
File: SDAQ_psim_types. Data types for the SDAQ_psim program.
Copyright (C) 12019-12021  Sam harry Tzavaras

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 3 of the License, or any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <pthread.h>
#include "SDAQ_drv.h"

//Run flag
extern unsigned char SDAQ_psim_run;
// Array of Mutex one for each pseudo SDAQ mem space, Used to block simultaneous access between UI and pseudo SDAQ thread
extern pthread_mutex_t *SDAQs_mem_access;

enum pSDAQ_flags_mask{
	disable = 0,
	cal_dates_send,
	info_send
};

//struct definition of memory space of a pseudo_SDAQ
typedef struct pSDAQ_memory_space_struct{
	unsigned short noise;
	unsigned short nosensor;
	unsigned short out_of_range;
	unsigned short over_range;
	unsigned char status;// status byte of the pseudo_SDAQ
	unsigned char pSDAQ_flags;// flags of the pSDAQ_flags
	unsigned int status_send_cnt;//counter, that when is 0 a device ID/ status message transmitted
	unsigned char address;
	unsigned char number_of_channels;
	float out_val[SDAQ_MAX_AMOUNT_OF_CHANNELS];
	sdaq_calibration_date ch_cal_date[SDAQ_MAX_AMOUNT_OF_CHANNELS];
	float data_cal_values[SDAQ_MAX_AMOUNT_OF_CHANNELS][MAX_AMOUNT_OF_POINTS][MAX_DATA_ON_POINT];
}pSDAQ_memory_space;

