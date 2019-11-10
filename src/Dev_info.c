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

/*	struct used in mode info and calibration. 
	Contains:
		 Struct SDAQ info. 
		 A List with calibration points data (aka sdaq_calibration_data) for each channel
		 A list with dates and amount of data (aka sdaq_calibration_date) for each channel
*/
typedef struct SDAQ_information_and_calibration_data
{
	struct SDAQ_info{
		unsigned int serial_number;
		const char *dev_type;	
		unsigned char firm_rev;
		unsigned char hw_rev;
		unsigned char num_of_ch;
		unsigned char sample_rate;
	}SDAQ_info;
	struct GSList *Calibration_date;
	struct GSlist *Calibration_point_data;
}SDAQ_info_cal_data;

//local functions
int get_SDAQ_info_data(int socket_num, unsigned char dev_addr, SDAQ_info_cal_data *str);
int get_SDAQ_calibration_data(int socket_num, unsigned char dev_addr, SDAQ_info_cal_data *str);

int info(int socket_num, unsigned char dev_addr, opt_flags *usr_flag)
{

	//Local variables, SDAQ information and calibration date and data.
	SDAQ_info_cal_data str={0}; 
	//Ask SDAQ for it's info.
	QueryDeviceInfo(socket_num, dev_addr);
	get_SDAQ_info_data(socket_num, dev_addr, &str);
	printf("Info of SDAQ with Address %d\n"
		   "     HW rev: %d\n"
		   "     SW rev: %d\n"
		   "        S/N: %d\n"
		   "       Type: %s\n"
		   "   Channels: %d\n"
		   " Samplerate: %d\n",dev_addr,
		   					 str.SDAQ_info.hw_rev,
		   					 str.SDAQ_info.firm_rev,
		   					 str.SDAQ_info.serial_number,
		   					 str.SDAQ_info.dev_type,
		   					 str.SDAQ_info.num_of_ch,
		   					 str.SDAQ_info.sample_rate);
	//QueryCalibrationData(socket_num, dev_addr);

	return EXIT_SUCCESS;
}

int get_SDAQ_info_data(int socket_num, unsigned char dev_addr, SDAQ_info_cal_data *str)
{
	//CAN Socket and SDAQ related variables
	struct can_frame frame_rx;
	int RX_bytes,c=2;
	sdaq_can_id *id_dec = (sdaq_can_id *)&(frame_rx.can_id); 
	sdaq_status *status_dec = (sdaq_status *)frame_rx.data;
	sdaq_info   *info_dec   = (sdaq_info *)frame_rx.data;
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
						str->SDAQ_info.serial_number = status_dec->dev_sn;
						str->SDAQ_info.dev_type = dev_type_str[status_dec->dev_type];
						c--; 
						break;
					case Device_info:
						str->SDAQ_info.num_of_ch = info_dec->num_of_ch;
						str->SDAQ_info.sample_rate = info_dec->sample_rate;
						str->SDAQ_info.hw_rev = info_dec->hw_rev;
						str->SDAQ_info.firm_rev = info_dec->firm_rev;
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
	return 0;
}
int get_SDAQ_calibration_data(int socket_num, unsigned char dev_addr, SDAQ_info_cal_data *str)
{
	/*
	//CAN Socket and SDAQ related variables
	struct can_frame frame_rx;
	int RX_bytes,c=2;
	sdaq_can_id *id_dec = (sdaq_can_id *)&(frame_rx.can_id); 
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
	*/
	return 0;
}



