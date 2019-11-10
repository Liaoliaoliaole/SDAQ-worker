#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <ncurses.h>
#include <glib.h> 
#include <gmodule.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "SDAQ_drv.h"
#include "Modes.h"

//local functions

int Dev_info(int socket_num, unsigned char dev_addr, opt_flags *usr_flag)
{
	//CAN Socket and SDAQ related variables
	struct can_frame frame_rx;
	int RX_bytes,c=2;
	sdaq_can_id *id_dec = (sdaq_can_id *)&(frame_rx.can_id); 
	sdaq_calibration_date *cal_date_dec = (sdaq_calibration_date *)&(frame_rx.data);
	sdaq_calibration_data *cal_data_dec = (sdaq_calibration_data *)&(frame_rx.data);
	
	//Local variables, SDAQ information and calibration date and data.
	sdaq_status w_SDAQ_status;
	sdaq_info w_SDAQ_info;
	/*
	sdaq_calibration_date *SDAQ_channels_calibration_date;
	sdaq_calibration_data *SDAQ_channels_calibration_data;
	*/
	//Ask SDAQ for it's info.
	QueryDeviceInfo(socket_num, dev_addr);
	while(c)//read the CANbus for 2 times to get status and info messages
	{
		RX_bytes=read(socket_num, &frame_rx, sizeof(frame_rx));
		if(RX_bytes==sizeof(frame_rx))
		{
			if(id_dec->device_addr==dev_addr)
			{
				switch(id_dec->payload_type)
				{
					case Device_status:
						memcpy(&w_SDAQ_status, frame_rx.data, sizeof(sdaq_status));
						c--; 
						break;
					case Device_info:
						memcpy(&w_SDAQ_info, frame_rx.data, sizeof(sdaq_info));
						c--; 
						break;
				}
			}
		}
		else
		{
			printf("....Timeout\n");
			return EXIT_FAILURE;
		}
	}
	c = w_SDAQ_info.num_of_ch;
	printf("Info of SDAQ with Address %d\n"
		   "     HW rev: %d\n"
		   "     SW rev: %d\n"
		   "        S/N: %d\n"
		   "       Type: %s\n"
		   "   Channels: %d\n"
		   " Samplerate: %d\n",dev_addr,
		   					 w_SDAQ_info.hw_rev,
		   					 w_SDAQ_info.firm_rev,
		   					 w_SDAQ_status.dev_sn,
		   					 dev_type_str[w_SDAQ_status.dev_type],
		   					 w_SDAQ_info.num_of_ch,
		   					 w_SDAQ_info.sample_rate);
	QueryCalibrationData(socket_num, dev_addr);
	while(c)//read the CANbus for c(aka number of channels) times to get the calibration data
	{
		RX_bytes=read(socket_num, &frame_rx, sizeof(frame_rx));
		if(RX_bytes==sizeof(frame_rx))
		{
			if(id_dec->device_addr==dev_addr)
			{
				switch(id_dec->payload_type)
				{
					case Device_status:
						memcpy(&w_SDAQ_status, frame_rx.data, sizeof(sdaq_status));
						c--; 
						break;
					case Device_info:
						memcpy(&w_SDAQ_info, frame_rx.data, sizeof(sdaq_info));
						c--; 
						break;
				}
			}
		}
		else
		{
			printf("....Timeout\n");
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
