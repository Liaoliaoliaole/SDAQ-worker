/*
File: SDAQ_drv.c, Implementation of SDAQ driver.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#include <arpa/inet.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#include "SDAQ_drv.h"

const unsigned char Parking_address=63;
const unsigned char Broadcast=0;
const unsigned char Unit_code_base_region_size=20;
const char *unit_str[256]={
//Base units
"-","V","mA","°C","Pa","mV","Ohm","","","","","","","","","","","","","",
//Specific units
"V","uV","mV","kV",//Voltage
"A","uA","mA","kA",//Amperage
"°C",//Temperature
"bar","barg","Pa","kPa","MPa","GPa",//Pressure
"um/m",//Strain
"N","kN","MN",//Force
"m","um","mm","cm","dm",//Displacement
"m/s","mm/s","km/h",//Velocity
"m/s2","gee",//Acceleration
"Ohm","kOhm","MOhm",//Resistance
"Nm","kNm","MNm",//Torque
"kg","gram","Tonn",//Mass
"deg","rad",//Angle
"Hz","kHz","MHz","rpm",//Frequency
"rad/s2","deg/s2",//Angular Acceleration
"rad/s","deg/s",//Angular Velocity
"kg/s","kg/min","kg/h",//Mass Flow
"m3/s","m3/min","m3/h","l/s","l/min","l/h",//Volumetric flow
"%",//percentage
"W","kW","MW",//Power
"J","kJ","MJ","Wh","kWh","MWh",//Energy
"mV/V","mV/mA",//Ratio
"l","m3",//Volume
/*Extra*/
"mbar",
};

const char *dev_type_str[SDAQ_MAX_DEV_NUM]={
	"Pseudo_SDAQ",
	"SDAQ-TC1",
	"SDAQ-TC16",
	"SDAQ-RTD",
	"SDAQ-I",
	"SDAQ-U"
};

const char *channel_status_str[]={
	"No Sensor",
	"Out of Range",
	"Over Range"
};

const char *type_of_point_str[]={
	"Unclassified",
	"Measure",
	"Reference",
	"Offset",
	"Gain",
	"C2",
	"C3"
};

const char *dev_input_mode_str[20][8]={
	{NULL},
	{"B","E","J","K","N","R","S","T"},
	{NULL},
	{"PT100-2W","PT100-3W","PT100-4W",
	 "PT1000-2W","PT1000-3W","PT1000-4W"},
	{NULL},
	{"2V","100V"}
};

const char *SDAQ_reg_status_str[SDAQ_registeration_max_state+1]={
	"Not register",
	"Initial Registration",
	"Pending Calibration data",
	"Pending input mode",
	"Done"
};

//Decoder for the status byte field from "Measure" message
const char * Channel_status_byte_dec(unsigned char status_byte)
{
		if(status_byte & (1<<No_sensor))
			return channel_status_str[0];
		if(status_byte & (1<<Out_of_range))
			return channel_status_str[1];
		if(status_byte & (1<<Over_range))
			return channel_status_str[2];
		return "Unclassified bit";
}

const char *dev_status_str[][8]={
{"Stand-By","No","No","","","","","Normal"},
{"Measuring","Yes","Yes","","","","","Booting"}};

//Indexing Strings, used to get SW revision and device type from firmware.
const char DEVID_indexer_str_550X[] = "SDAQ_550X_";
const char DEVID_indexer_str[] = "@DeviceId@";
const char REV_indexer_str[] = "@Revision@";
const unsigned int DEVID_indexer_str_550X_len = strlen(DEVID_indexer_str_550X);
const unsigned int DEVID_indexer_str_len = strlen(DEVID_indexer_str);
const unsigned int REV_indexer_str_len = strlen(REV_indexer_str);

//Decoder for the status byte field from "Device_ID/Status" message
const char * status_byte_dec(unsigned char status_byte,unsigned char field)
{
	switch (field)
	{
		case State:
			return status_byte & (1<<State) ? dev_status_str[1][State] : dev_status_str[0][State];
		case In_sync:
			return status_byte & (1<<In_sync) ? dev_status_str[1][In_sync] : dev_status_str[0][In_sync];
		case Error:
			return status_byte & (1<<Error) ? dev_status_str[1][Error] : dev_status_str[0][Error];
		case Mode:
			return status_byte & (1<<Mode) ? dev_status_str[1][Mode] : dev_status_str[0][Mode];
		default :
			return "";
	}
}

				/*TX Functions*/
//Synchronize the SDAQ devices. Requested by broadcast only.
int Sync(int socket_fd, unsigned short time_seed)
{
	struct can_frame frame_tx = {0};
	sdaq_can_id *sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);

	//construct identifier for synchronization measure message
	sdaq_id_ptr->flags = 4;//set the EFF
	sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	sdaq_id_ptr->payload_type = Synchronization_command;//Payload type for synchronization command
	sdaq_id_ptr->device_addr = 0;//TX from broadcast only
	frame_tx.can_dlc = sizeof(short);//Payload size
	*((unsigned short *)frame_tx.data) = time_seed;
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))>0)
		return 1;
	return 0;
}
//Request start of measure from the SDAQ device. For all dev_addr=0
int Start(int socket_fd,unsigned char dev_address)
{
	struct can_frame frame_tx = {0};
	sdaq_can_id *sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);

	//construct identifier for start measure message
	sdaq_id_ptr->flags = 4;//set the EFF
	sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	sdaq_id_ptr->payload_type = Start_command;//Payload type for start measure command
	sdaq_id_ptr->device_addr = dev_address;
	frame_tx.can_dlc = 0;//No payload
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}
//Request stop of measure from the SDAQ device. For all dev_addr=0
int Stop(int socket_fd,unsigned char dev_address)
{
	struct can_frame frame_tx = {0};
	sdaq_can_id *sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);

	//construct identifier for stop measure message
	sdaq_id_ptr->flags = 4;//set the EFF
	sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	sdaq_id_ptr->payload_type = Stop_command;//Payload type for stop measure command
	sdaq_id_ptr->device_addr = dev_address;
	frame_tx.can_dlc = 0;//No payload
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}
//request change of device address with the specific serial number.
int SetDeviceAddress(int socket_fd,unsigned int dev_SN, unsigned char new_dev_address)
{
	struct can_frame frame_tx = {0};
	sdaq_can_id *sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);

	//construct identifier for change of device address message
	sdaq_id_ptr->flags = 4;//set the EFF
	sdaq_id_ptr->priority = 4;//From the SDAQ White paper
	sdaq_id_ptr->protocol_id=PROTOCOL_ID;
	sdaq_id_ptr->payload_type=Set_dev_address;//Payload type for change of device address command
	sdaq_id_ptr->device_addr=0;//TX from broadcast only
	frame_tx.can_dlc = sizeof(unsigned int) + sizeof(unsigned char);//Payload size
	*((int *)frame_tx.data) = dev_SN;
	*(frame_tx.data + sizeof(unsigned int)) = new_dev_address;
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}
//request device info. Device answer with 3 messages: Device ID/status, Device Info and Calibration Date.
int QueryDeviceInfo(int socket_fd,unsigned char dev_address)
{
	struct can_frame frame_tx = {0};
	sdaq_can_id *sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);

	//construct identifier for device info request command
	sdaq_id_ptr->flags = 4;//set the EFF
	sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	sdaq_id_ptr->payload_type=Query_Dev_info;//Payload type for device info request command
	sdaq_id_ptr->device_addr=dev_address;
	frame_tx.can_dlc = 0;//No payload
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}

int QueryCalibrationData(int socket_fd, unsigned char dev_address, unsigned char channel)
{
	struct can_frame frame_tx = {0};
	sdaq_can_id *sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);

	//construct identifier for Query_Calibration_Data message
	sdaq_id_ptr->flags = 4;//set the EFF
	sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	sdaq_id_ptr->payload_type = Query_Calibration_Data;//Payload type for Query_Calibration_Data message
	sdaq_id_ptr->device_addr = dev_address;
	sdaq_id_ptr->channel_num = channel;
	frame_tx.can_dlc = 0;//No Payload
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}

//Request system variables. Device answer with all the system variables of the SDAQ.
int QuerySystemVariables(int socket_fd, unsigned char dev_address)
{
	struct can_frame frame_tx = {0};
	sdaq_can_id *sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);

	//construct identifier for Query_Calibration_Data message
	sdaq_id_ptr->flags = 4;//set the EFF
	sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	sdaq_id_ptr->payload_type = Query_system_variables;//Payload type for Query_system_variables message
	sdaq_id_ptr->device_addr = dev_address;
	frame_tx.can_dlc = 0;//No Payload
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}

//Control Configure Additional data. If Device is in measure will transmit raw measurement message
int Req_Raw_meas(int socket_fd,unsigned char dev_address,const unsigned char Config)
{
	struct can_frame frame_tx = {0};
	sdaq_can_id *sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);

	//construct identifier for "Configure Additional data" command
	sdaq_id_ptr->flags = 4;//set the EFF
	sdaq_id_ptr->priority = 4;//From the SDAQ White paper
	sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	sdaq_id_ptr->payload_type = Configure_Additional_data;//Payload type for "Configure Additional data" command
	sdaq_id_ptr->device_addr = dev_address;
	frame_tx.can_dlc = 1;//Payload size
	frame_tx.data[0] = Config;
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}

//Write the calibration date data of the channel 'channel_num' of the SDAQ with address 'dev_address'
int WriteCalibrationDate(int socket_fd, unsigned char dev_address, unsigned char channel_num, void *date_ptr, unsigned char period, unsigned char NumOfPoints, unsigned char unit)
{
	struct can_frame frame_tx = {0};
	struct tm *date = date_ptr;
	sdaq_calibration_date *sdaq_cal_date_enc = (sdaq_calibration_date*) frame_tx.data;
	sdaq_can_id *sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);

	//construct identifier for "Write_calibration_Date" command
	sdaq_id_ptr->flags = 4;//set the EFF
	sdaq_id_ptr->priority = 4;//From the SDAQ White paper
	sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	sdaq_id_ptr->payload_type = Write_calibration_Date;//Payload type for "Write_calibration_Date" command
	sdaq_id_ptr->device_addr = dev_address;
	sdaq_id_ptr->channel_num = channel_num;
	frame_tx.can_dlc = sizeof(sdaq_calibration_date);//Payload size
	sdaq_cal_date_enc->year = date->tm_year - 100;//100 = 2000-1900
	sdaq_cal_date_enc->month = date->tm_mon + 1;
	sdaq_cal_date_enc->day = date->tm_mday;
	sdaq_cal_date_enc->period = period;
	sdaq_cal_date_enc->amount_of_points = NumOfPoints;
	sdaq_cal_date_enc->cal_units = unit;
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	usleep(10000);
	return 0;
}
//Write the calibration point data 'NumOfPoint' of the channel 'channel_num' of the SDAQ with address 'dev_address'
int WriteCalibrationPoint(int socket_fd, unsigned char dev_address, unsigned char channel_num, float point_val, unsigned char point_num, unsigned char type)
{
	struct can_frame frame_tx = {0};
	sdaq_calibration_points_data *sdaq_cal_point_data_enc = (sdaq_calibration_points_data*) frame_tx.data;
	sdaq_can_id *sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);

	//construct identifier for "Write_calibration_Date" command
	sdaq_id_ptr->flags = 4;//set the EFF
	sdaq_id_ptr->priority = 4;//From the SDAQ White paper
	sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	sdaq_id_ptr->payload_type = Write_calibration_Point_Data;//Payload type for "Write_calibration_Point_Data" command
	sdaq_id_ptr->device_addr = dev_address;
	sdaq_id_ptr->channel_num = channel_num;
	frame_tx.can_dlc = sizeof(sdaq_calibration_points_data);//Payload size
	sdaq_cal_point_data_enc->data_of_point = point_val;
	sdaq_cal_point_data_enc->type = type;
	sdaq_cal_point_data_enc->points_num = point_num;
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	usleep(10000);
	return 0;
}

		/*--- SDAQ's Bootloader related functions ---*/
//Set execution code of SDAQ's uC.
int SDAQ_goto(int socket_fd, unsigned char dev_address, _Bool code_reg_fl)
{
	struct can_frame frame_tx = {0};
	sdaq_can_id *sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);

	//construct identifier for "GOTO" command.
	sdaq_id_ptr->flags = 4;//set the EFF
	sdaq_id_ptr->priority = 0;
	sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	sdaq_id_ptr->payload_type = code_reg_fl ? goto_bootloader : goto_application;
	sdaq_id_ptr->device_addr = dev_address;
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}
//Erase SDAQ's Flash memory region.
int SDAQ_erase_flash(int socket_fd, unsigned char dev_address, unsigned int start_addr, unsigned int last_addr)
{
	struct can_frame frame_tx = {0};
	sdaq_flash_erase *sdaq_flash_erase_enc = (sdaq_flash_erase*) frame_tx.data;
	sdaq_can_id *sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);

	//construct identifier for "Erase_flash" command.
	sdaq_id_ptr->flags = 4;//set the EFF
	sdaq_id_ptr->priority = 0;
	sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	sdaq_id_ptr->payload_type = Erase_flash;
	sdaq_id_ptr->device_addr = dev_address;
	//construct payload
	frame_tx.can_dlc = sizeof(sdaq_flash_erase);//Payload size
	sdaq_flash_erase_enc->Start_addr = start_addr;
	sdaq_flash_erase_enc->End_addr = last_addr;
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}
//Write firmware image header to SDAQ.
int SDAQ_write_header(int socket_fd, unsigned char dev_address, unsigned int start_addr, unsigned int range, unsigned int crc, unsigned char *ret_buff)
{
	int retval;
	SDAQ_img_header *header;

	header = !ret_buff ? malloc(sizeof(SDAQ_img_header)) : ret_buff;
	header->header_word = SDAQ_IMG_HEADER_WORD + (start_addr<0x10000?1:0); // From whitepaper. New devices (program memory start @0x7000) have other magic number.
	header->start_addr = start_addr;
	header->end_addr = start_addr + range-1;
	header->crc = crc;
	memset(header->reserved, -1, sizeof(header->reserved));
	retval = SDAQ_write_page_buff(socket_fd, dev_address, (unsigned char *)header);
	if(!ret_buff)
		free(header);
	return retval;
}
//Write to page buffer.
int SDAQ_write_page_buff(int socket_fd, unsigned char dev_address, unsigned char *data)
{
	struct can_frame frame_tx = {0};
	sdaq_can_id *sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);

	//construct identifier for "Write_to_page_buff" command.
	sdaq_id_ptr->flags = 4;//set the EFF
	sdaq_id_ptr->priority = 0;
	sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	sdaq_id_ptr->payload_type = Write_to_page_buff;
	sdaq_id_ptr->device_addr = dev_address;
	frame_tx.can_dlc = 8;//Maximum Payload size
	//Load data to payload and send
	do{
		memcpy(frame_tx.data, data, frame_tx.can_dlc);
		usleep(1000);//Delay to prevent FIFO overflow.
		if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
			return 1;
		sdaq_id_ptr->channel_num++;
		data += frame_tx.can_dlc;
	}while(sdaq_id_ptr->channel_num<PAGE_SECTIONS);
	return 0;
}
//Transfer page buffer to Flash memory.
int SDAQ_Transfer_to_flash(int socket_fd, unsigned char dev_address, unsigned int addr)
{
	struct can_frame frame_tx = {0};
	sdaq_transfer_buffer *sdaq_transfer_buffer_enc = (sdaq_transfer_buffer*) frame_tx.data;
	sdaq_can_id *sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);

	//construct identifier for "Write_page_buff_to_flash" command.
	sdaq_id_ptr->flags = 4;//set the EFF
	sdaq_id_ptr->priority = 0;
	sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	sdaq_id_ptr->payload_type = Write_page_buff_to_flash;
	sdaq_id_ptr->device_addr = dev_address;
	//construct payload
	frame_tx.can_dlc = sizeof(sdaq_transfer_buffer);//Payload size
	sdaq_transfer_buffer_enc->addr = addr;
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}

/*-----------------------------------------------------------------------------------------------------------------*/

/*The following Functions used only on the pseudo_SDAQ Simulator*/
int p_debug_data(int socket_fd, unsigned char dev_address, unsigned short ref_time, unsigned short dev_time)
{
	sdaq_can_id *p_sdaq_id_ptr;
	sdaq_sync_debug_data *p_sdaq_sync_debug_data;
	struct can_frame frame_tx = {0};
	p_sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(p_sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	memset(frame_tx.data, 0, sizeof(frame_tx.data));
	//construct identifier for Device_status message
	p_sdaq_id_ptr->flags = 4;//set the EFF
	p_sdaq_id_ptr->priority = 7;//According to the White paper
	p_sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	p_sdaq_id_ptr->payload_type = Sync_Info;//Payload type for Device_status message
	p_sdaq_id_ptr->device_addr = dev_address;
	frame_tx.can_dlc = 4;//Payload size from the white paper is 8
	p_sdaq_sync_debug_data = (sdaq_sync_debug_data*) (frame_tx.data);
	p_sdaq_sync_debug_data->ref_time = ref_time;
	p_sdaq_sync_debug_data->dev_time = dev_time;
	usleep(10000);//hack to prevent message lost in case that the CAN-IF is real.
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}

int p_DeviceID_and_status(int socket_fd,unsigned char dev_address, unsigned int SN, unsigned char status)
{
	sdaq_can_id *p_sdaq_id_ptr;
	sdaq_status *p_sdaq_status;
	struct can_frame frame_tx = {0};
	p_sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(p_sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	//construct identifier for Device_status message
	p_sdaq_id_ptr->flags = 4;//set the EFF
	p_sdaq_id_ptr->priority = 4;//According to the White paper
	p_sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	p_sdaq_id_ptr->payload_type = Device_status;//Payload type for Device_status message
	p_sdaq_id_ptr->device_addr = dev_address;
	frame_tx.can_dlc = sizeof(sdaq_status);//Payload size
	p_sdaq_status = (sdaq_status*) (frame_tx.data);
	p_sdaq_status -> dev_sn = SN;
	p_sdaq_status -> status = status;
	p_sdaq_status -> dev_type = 0;
	usleep(10000);//hack to prevent message lost in case that the CAN-IF is real.
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}

int p_DeviceInfo(int socket_fd, unsigned char dev_address, unsigned char amount_of_channel)
{
	sdaq_can_id *p_sdaq_id_ptr;
	sdaq_info *p_sdaq_info;
	struct can_frame frame_tx = {0};
	p_sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(p_sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	//construct identifier for Device_status message
	p_sdaq_id_ptr->flags = 4;//set the EFF
	p_sdaq_id_ptr->priority = 4;//According to the White paper
	p_sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	p_sdaq_id_ptr->payload_type = Device_info;//Payload type for Device_info message
	p_sdaq_id_ptr->device_addr = dev_address;
	frame_tx.can_dlc = sizeof(sdaq_info);//Payload size
	p_sdaq_info = (sdaq_info*) (frame_tx.data);
	p_sdaq_info -> dev_type = 0;
	p_sdaq_info -> firm_rev = 0;
	p_sdaq_info -> hw_rev = 0;
	p_sdaq_info -> num_of_ch = amount_of_channel;
	p_sdaq_info -> sample_rate = 10;
	p_sdaq_info -> max_cal_point = 16;
	usleep(10000);//hack to prevent message lost in case that the CAN-IF is real.
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}

int p_measure(int socket_fd, unsigned char dev_address, unsigned char channel, unsigned char state, unsigned char unit, float value, unsigned short timestamp)
{
	sdaq_can_id *p_sdaq_id_ptr;
	sdaq_meas *p_sdaq_meas;
	struct can_frame frame_tx = {0};
	p_sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(p_sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	//construct identifier for Device_status message
	p_sdaq_id_ptr->flags = 4;//set the EFF
	p_sdaq_id_ptr->priority = 3;//According to the White paper
	p_sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	p_sdaq_id_ptr->payload_type = Measurement_value;//Payload type for Device_measurement message
	p_sdaq_id_ptr->device_addr = dev_address;
	p_sdaq_id_ptr->channel_num = channel;
	frame_tx.can_dlc = sizeof(sdaq_meas);//Payload size
	p_sdaq_meas = (sdaq_meas*) (frame_tx.data);
	p_sdaq_meas -> meas = value;
	p_sdaq_meas -> unit = unit;
	p_sdaq_meas -> status = state;
	p_sdaq_meas -> timestamp = timestamp;
	usleep(1000);//hack to prevent message lost in case that the CAN-IF is real.
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}

int p_measure_raw(int socket_fd, unsigned char dev_address, unsigned char channel, unsigned char state, float value, unsigned short timestamp)
{
	sdaq_can_id *p_sdaq_id_ptr;
	sdaq_meas *p_sdaq_meas;
	struct can_frame frame_tx = {0};
	p_sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(p_sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	//construct identifier for Device_status message
	p_sdaq_id_ptr->flags = 4;//set the EFF
	p_sdaq_id_ptr->priority = 3;//According to the White paper
	p_sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	p_sdaq_id_ptr->payload_type = Uncalibrated_meas;//Payload type for Device_measurement message
	p_sdaq_id_ptr->device_addr = dev_address;
	p_sdaq_id_ptr->channel_num = channel;
	frame_tx.can_dlc = sizeof(sdaq_meas);//Payload size
	p_sdaq_meas = (sdaq_meas*) (frame_tx.data);
	p_sdaq_meas -> meas = value;
	p_sdaq_meas -> unit = 0;
	p_sdaq_meas -> status = state;
	p_sdaq_meas -> timestamp = timestamp;
	usleep(1000);//hack to prevent message lost in case that the CAN-IF is real.
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}

int p_calibration_date(int socket_fd, unsigned char dev_address, unsigned char channel, sdaq_calibration_date *ch_cal_date)
{
	sdaq_can_id *p_sdaq_id_ptr;
	struct can_frame frame_tx = {0};
	p_sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(p_sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	//construct identifier for Device_status message
	p_sdaq_id_ptr->flags = 4;//set the EFF
	p_sdaq_id_ptr->priority = 4;//According to the White paper
	p_sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	p_sdaq_id_ptr->payload_type = Calibration_Date;//Payload type for Calibration_Date message
	p_sdaq_id_ptr->device_addr = dev_address;
	p_sdaq_id_ptr->channel_num = channel;
	frame_tx.can_dlc = sizeof(sdaq_calibration_date);//Payload size
	memcpy(frame_tx.data, ch_cal_date, sizeof(sdaq_calibration_date));
	usleep(1000);//hack to prevent message lost in case that the CAN-IF is real.
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}

int p_calibration_points_data(int socket_fd, unsigned char dev_address, unsigned char channel, sdaq_calibration_points_data *ch_cal_point_data)
{
	sdaq_can_id *p_sdaq_id_ptr;
	struct can_frame frame_tx = {0};
	p_sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(p_sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	//construct identifier for Device_status message
	p_sdaq_id_ptr->flags = 4;//set the EFF
	p_sdaq_id_ptr->priority = 4;//According to the White paper
	p_sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	p_sdaq_id_ptr->payload_type = Calibration_Point_Data;//Payload type for Calibration_Point_Data message
	p_sdaq_id_ptr->device_addr = dev_address;
	p_sdaq_id_ptr->channel_num = channel;
	frame_tx.can_dlc = sizeof(sdaq_calibration_points_data);//Payload size
	memcpy(frame_tx.data, ch_cal_point_data, sizeof(sdaq_calibration_points_data));
	usleep(1000);//hack to prevent message lost in case that the CAN-IF is real.
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}
