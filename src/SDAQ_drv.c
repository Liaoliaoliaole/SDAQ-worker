#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <string.h> 

#include <linux/can.h>
#include <linux/can/raw.h>

#include "SDAQ_drv.h"

const char *unit_str[]={"\\Q/","V","A","°C","Pa","mV"};  
const char *dev_type_str[]={"Pseudo_SDAQ","SDAQ-TC-1","SDAQ-TC-16","SDAQ-PT100-1"};
const char *dev_status_str[][8]={{"Stand-By","No","No","","","","","Normal"},{"Measuring","Yes","Yes","","","","","Booting"}};  
const unsigned char Parking_address=63;
const unsigned char Broadcast=0;

//Decoder for the status byte field from "CAN Device_ID/Status" message
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
	sdaq_id_ptr->priority=4;//From the SDAQ White paper
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

int QueryCalibrationData(int socket_fd, unsigned char dev_address)
{
	sdaq_can_id *p_sdaq_id_ptr;
	struct can_frame frame_tx;
	p_sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(p_sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	//construct identifier for Query_Calibration_Data message
	p_sdaq_id_ptr->flags=4;//set the EFF
	p_sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	p_sdaq_id_ptr->payload_type = Query_Calibration_Data;//Payload type for Query_Calibration_Data message
	p_sdaq_id_ptr->device_addr = dev_address;
	frame_tx.can_dlc = 0;//No Payload 
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;	
}

//Control Configure Additional data. If Device is in measure will transmit raw measurement message
int Req_Raw_meas(int socket_fd,unsigned char dev_address,const unsigned char Config)
{
	sdaq_can_id *sdaq_id_ptr;
	struct can_frame frame_tx;
	sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	//construct identifier for "Configure Additional data" command
	sdaq_id_ptr->flags=4;//set the EFF
	sdaq_id_ptr->priority=4;//From the SDAQ White paper
	sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	sdaq_id_ptr->payload_type = Configure_Additional_data;//Payload type for "Configure Additional data" command
	sdaq_id_ptr->device_addr = dev_address;
	frame_tx.can_dlc = 1;//Payload size
	frame_tx.data[0] = Config;
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}

//The following RX Functions used on the pseudo_SDAQ Simulator 
				/*RX Functions*/
int p_DeviceID_and_status(int socket_fd,unsigned char dev_address, unsigned int SN, unsigned char status)
{
	sdaq_can_id *p_sdaq_id_ptr;
	sdaq_status *p_sdaq_status;
	struct can_frame frame_tx;
	p_sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(p_sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	//construct identifier for Device_status message
	p_sdaq_id_ptr->flags=4;//set the EFF
	p_sdaq_id_ptr->priority=4;//According to the White paper
	p_sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	p_sdaq_id_ptr->payload_type = Device_status;//Payload type for Device_status message
	p_sdaq_id_ptr->device_addr = dev_address;
	frame_tx.can_dlc = sizeof(sdaq_status);//Payload size
	p_sdaq_status = (sdaq_status*) &(frame_tx.data);
	p_sdaq_status -> dev_sn = SN;
	p_sdaq_status -> status = status;
	p_sdaq_status -> dev_type = 0;
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;
}

int p_DeviceInfo(int socket_fd, unsigned char dev_address, unsigned char amount_of_channel)
{
	sdaq_can_id *p_sdaq_id_ptr;
	sdaq_info *p_sdaq_info;
	struct can_frame frame_tx;
	p_sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(p_sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	//construct identifier for Device_status message
	p_sdaq_id_ptr->flags=4;//set the EFF
	p_sdaq_id_ptr->priority=4;//According to the White paper
	p_sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	p_sdaq_id_ptr->payload_type = Device_info;//Payload type for Device_info message
	p_sdaq_id_ptr->device_addr = dev_address;
	frame_tx.can_dlc = sizeof(sdaq_info);//Payload size
	p_sdaq_info = (sdaq_info*) &(frame_tx.data);
	p_sdaq_info -> dev_type = 0;
	p_sdaq_info -> firm_rev = 0;
	p_sdaq_info -> hw_rev = 0;
	p_sdaq_info -> num_of_ch = amount_of_channel;
	p_sdaq_info -> sample_rate = 0;
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;	
}

int p_measure(int socket_fd, unsigned char dev_address, unsigned char channel, float value,unsigned short timestamp)
{
	sdaq_can_id *p_sdaq_id_ptr;
	sdaq_meas *p_sdaq_meas;
	struct can_frame frame_tx;
	p_sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(p_sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	//construct identifier for Device_status message
	p_sdaq_id_ptr->flags=4;//set the EFF
	p_sdaq_id_ptr->priority=3;//According to the White paper
	p_sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	p_sdaq_id_ptr->payload_type = Measurement_value;//Payload type for Device_measurement message
	p_sdaq_id_ptr->device_addr = dev_address;
	p_sdaq_id_ptr->channel_num = channel;
	frame_tx.can_dlc = sizeof(sdaq_meas);//Payload size
	p_sdaq_meas = (sdaq_meas*) &(frame_tx.data);
	p_sdaq_meas -> meas = value;
	p_sdaq_meas -> unit = 0;
	p_sdaq_meas -> status = 0;
	p_sdaq_meas -> timestamp = timestamp;
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;	
}

int p_measure_raw(int socket_fd, unsigned char dev_address, unsigned char channel, float value,unsigned short timestamp)
{
	sdaq_can_id *p_sdaq_id_ptr;
	sdaq_meas *p_sdaq_meas;
	struct can_frame frame_tx;
	p_sdaq_id_ptr = (sdaq_can_id *)&(frame_tx.can_id);
	memset(p_sdaq_id_ptr, 0, sizeof(sdaq_can_id));
	//construct identifier for Device_status message
	p_sdaq_id_ptr->flags=4;//set the EFF
	p_sdaq_id_ptr->priority=3;//According to the White paper
	p_sdaq_id_ptr->protocol_id = PROTOCOL_ID;
	p_sdaq_id_ptr->payload_type = Uncalibrated_meas;//Payload type for Device_measurement message
	p_sdaq_id_ptr->device_addr = dev_address;
	p_sdaq_id_ptr->channel_num = channel;
	frame_tx.can_dlc = sizeof(sdaq_meas);//Payload size
	p_sdaq_meas = (sdaq_meas*) &(frame_tx.data);
	p_sdaq_meas -> meas = value;
	p_sdaq_meas -> unit = 0;
	p_sdaq_meas -> status = 0;
	p_sdaq_meas -> timestamp = timestamp;
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))<0)
		return 1;
	return 0;	
}
