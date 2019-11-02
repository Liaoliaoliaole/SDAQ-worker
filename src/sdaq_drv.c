#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <string.h> 

#include <arpa/inet.h> //for htonl,htons

#include <linux/can.h>
#include <linux/can/raw.h>

#include "sdaq_drv.h"
/*
// enumerator for payload_type
enum payload_type
{
	Synchronization_command = 1,
	Start_command = 2,
	Stop_command = 3,
	Set_dev_address = 6,
	Query_Dev_info = 7,
	Query_Calibration_Data = 8,
	Write_calibration_Date = 9,
	Write_calibration_Point_Data = 10,
	Configure_Additional_data = 0x0C,
	Measurement_value = 0x84, 
	Device_status = 0x86, 
	Device_info = 0x88,
	Calibration_Date = 0x89,
	Calibration_Point_Data = 0x8a,
	Uncalibrated_meas = 0x8b,
	Bootloader_Reply = 0xa0,
	Page_Buffer_Data = 0xa1,
	Sync_Info = 0xc0
};

// SDAQ's CAN identifier encoder/decoder 
typedef struct{
	unsigned flags : 3;//EFF/RTR/ERR flags
	unsigned priority : 3;
	unsigned protocol_id : 6;
	unsigned payload_type: 8;
	unsigned device_addr : 6;
	unsigned channel_num : 6;
}sdaq_can_id; 
*/

const char *unit_str[]={"","V","A","°C","Pa","mV"}; 
const char *dev_type_str[]={"","SDAQ-TC-1","SDAQ-TC-16","SDAQ-PT100-1"}; 

//Synchronize the SDAQ devices. Requested by broadcast only.
int Sync(int socket_fd, short time_seed)
{
	sdaq_can_id *sdaq_id_ptr;
	struct can_frame frame_tx;
	sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	//construct identifier for synchronization measure message
	sdaq_id_ptr->flags = 4;//set the EFF
	sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	sdaq_id_ptr->payload_type = Synchronization_command;//Payload type for synchronization command
	sdaq_id_ptr->device_addr = 0;//TX from broadcast only
	frame_tx.can_dlc = sizeof(short);//Payload size
	*((short *)frame_tx.data) = time_seed;
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))>0)
		return 1;
	return 0;	
}
//Request start of measure from the SDAQ device. For all dev_addr=0
int Start(int socket_fd,unsigned char dev_address)
{
	sdaq_can_id *sdaq_id_ptr;
	struct can_frame frame_tx;
	sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	//construct identifier for start measure message
	sdaq_id_ptr->flags=4;//set the EFF
	sdaq_id_ptr->protocol_id=PROTOCOL_ID;
	sdaq_id_ptr->payload_type=Start_command;//Payload type for start measure command
	sdaq_id_ptr->device_addr=dev_address;
	frame_tx.can_dlc = 0;//No payload
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}
//Request stop of measure from the SDAQ device. For all dev_addr=0
int Stop(int socket_fd,unsigned char dev_address)
{
	sdaq_can_id *sdaq_id_ptr;
	struct can_frame frame_tx;
	sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	//construct identifier for stop measure message
	sdaq_id_ptr->flags=4;//set the EFF
	sdaq_id_ptr->protocol_id=PROTOCOL_ID;
	sdaq_id_ptr->payload_type=Stop_command;//Payload type for stop measure command
	sdaq_id_ptr->device_addr=dev_address;
	frame_tx.can_dlc = 0;//No payload
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}
//request change of device address with the specific serial number.
int SetDeviceAddress(int socket_fd,unsigned int dev_SN, unsigned char new_dev_address)
{
	sdaq_can_id *sdaq_id_ptr;
	struct can_frame frame_tx;
	sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	//construct identifier for change of device address message
	sdaq_id_ptr->flags=4;//set the EFF
	sdaq_id_ptr->protocol_id=PROTOCOL_ID;
	sdaq_id_ptr->payload_type=Set_dev_address;//Payload type for change of device address command
	sdaq_id_ptr->device_addr=0;//TX from broadcast only
	frame_tx.can_dlc = sizeof(unsigned int) + sizeof(unsigned char);//Payload size
	*((int *)frame_tx.data) = htonl(dev_SN); //Endianness correction 
	*(frame_tx.data + sizeof(unsigned int)) = new_dev_address;
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;	
}
//request device info. Device answer with 3 messages: Device ID/status, Device Info and Calibration Date. 
int QueryDeviceInfo(int socket_fd,unsigned char dev_address)
{
	sdaq_can_id *sdaq_id_ptr;
	struct can_frame frame_tx;
	sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	//construct identifier for device info request command
	sdaq_id_ptr->flags=4;//set the EFF
	sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	sdaq_id_ptr->payload_type=Query_Dev_info;//Payload type for device info request command
	sdaq_id_ptr->device_addr=dev_address;
	frame_tx.can_dlc = 0;//No payload
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;	
}

//Control Configure Additional data. If Device is in measure will transmit raw measurement message
int Raw_meas(int socket_fd,unsigned char dev_address,const unsigned char Config)
{
	sdaq_can_id *sdaq_id_ptr;
	struct can_frame frame_tx;
	sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	//construct identifier for "Configure Additional data" command
	sdaq_id_ptr->flags=4;//set the EFF
	sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	sdaq_id_ptr->payload_type = Configure_Additional_data;//Payload type for "Configure Additional data" command
	sdaq_id_ptr->device_addr = dev_address;
	frame_tx.can_dlc = 1;//Payload size
	frame_tx.data[0] = Config;
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}
