/*   
Copyright (C) 12019-12020  Sam harry Tzavaras

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

#define PROTOCOL_ID 0x35

extern const char *unit_str[]; 
extern const char *dev_type_str[];
extern const char *dev_status_str[][8]; 
extern const unsigned char Parking_address;
extern const unsigned char Broadcast;

// enumerator for Status byte
enum status_byte{
	State =0,
	In_sync =1,
	Error =2,
	Mode = 7
};

// enumerator for Calibration Point Data type byte
enum Calibration_Point_Data_type_byte{
	meas = 1,
	ref = 2
};
// enumerator for payload_type
enum payload_type{
/*Messages payload_type. Master -> SDAQ*/
	Synchronization_command = 1,
	Start_command = 2,
	Stop_command = 3,
	Set_dev_address = 6,
	Query_Dev_info = 7,
	Query_Calibration_Data = 8,
	Write_calibration_Date = 9,
	Write_calibration_Point_Data = 10,
	Change_SDAQ_baudrate = 11,
	Configure_Additional_data = 12,
/*Messages payload_type. SDAQ -> Master*/
	Measurement_value = 0x84, 
	Device_status = 0x86, 
	Device_info = 0x88,
	Calibration_Date = 0x89,
	Calibration_Point_Data = 0x8a,
	Uncalibrated_meas = 0x8b,
	Sync_Info = 0xc0
};

#pragma pack(push, 1)//use pragma pack() to pack the following structs to 1 byte size (aka no zero padding)

/* SDAQ's CAN identifier encoder/decoder */
typedef struct SDAQ_Identifier_Encoder_Decoder{
	unsigned channel_num : 6;
	unsigned device_addr : 6;
	unsigned payload_type: 8;
	unsigned protocol_id : 6;
	unsigned priority : 3;
	unsigned flags : 3;//EFF/RTR/ERR flags
}sdaq_can_id; 

/* SDAQ's CAN measurement message decoder */
typedef struct SDAQ_Measurement_Decoder{
	float meas;
	unsigned char unit;
	unsigned char status;
	unsigned short timestamp;
}sdaq_meas; 

/* SDAQ's CAN Device_ID/Status message decoder */
typedef struct SDAQ_Status_Decoder{
	unsigned int  dev_sn;
	unsigned char status;
	unsigned char dev_type;
}sdaq_status; 

/* SDAQ's CAN Device_info message decoder */
typedef struct SDAQ_Info_Decoder{
	unsigned char dev_type;
	unsigned char firm_rev;
	unsigned char hw_rev;
	unsigned char num_of_ch;
	unsigned char sample_rate;
}sdaq_info; 

/* SDAQ's CAN Calibration_date message decoder */
typedef struct SDAQ_calibration_date_Decoder{
	unsigned int date;
	unsigned char amount_of_points;
}sdaq_calibration_date; 

/* SDAQ's CAN Calibration_point_data message decoder */
typedef struct SDAQ_calibration_points_data_Decoder{
	float data_of_point;
	unsigned char type;
	unsigned char points_num;
}sdaq_calibration_points_data; 


//The following RX Decoders used on the pseudo_SDAQ Simulator 
/* SDAQ's CAN Set Device Address message decoder */
typedef struct pSDAQ_Set_new_address{
	unsigned int  dev_sn;
	unsigned char new_address;
}sdaq_set_new_addr; 

/* SDAQ's CAN Debug data message decoder */
typedef struct pSDAQ_sync_debug_data{
	unsigned short ref_time;
	unsigned short dev_time;
}sdaq_sync_debug_data; 

#pragma pack(pop)//Disable packing 

				/*Master -> SDAQ Functions*/
/*All the functions return 0 in success and 1 on failure */
//Request start of measure from the SDAQ device. For all dev_addr=0
int Start(int socket_fd, unsigned char dev_address);
//Request stop of measure from the SDAQ device. For all dev_addr=0
int Stop(int socket_fd, unsigned char dev_address);
//Synchronize the SDAQ devices. Requested by broadcast only.
int Sync(int socket_fd, short time_seed);
//Control Configure Additional data. If Device is in measure will transmit raw measurement message
int Req_Raw_meas(int socket_fd, unsigned char dev_address, const unsigned char Config);
//Request change of device address with the specific serial number.
int SetDeviceAddress(int socket_fd, unsigned int dev_SN, unsigned char new_dev_address);
//Request device info. Device answer with 3 messages types: Device ID/status, Device Info and Calibration Date for each channel  
int QueryDeviceInfo(int socket_fd, unsigned char dev_address);
//Request calibration data. Device answer with 2 messages types: Calibration Date and Calibration Point Data for each channel 
int QueryCalibrationData(int socket_fd, unsigned char dev_address, unsigned char channel);
//Write the calibration date data of the channel 'channel_num' of the SDAQ with address 'dev_address'
int WriteCalibrationDate(int socket_fd, unsigned char dev_address, unsigned char channel_num, unsigned int date,unsigned char NumOfPoints);
//Write the calibration point data 'NumOfPoint' of the channel 'channel_num' of the SDAQ with address 'dev_address'
int WriteCalibrationPoint(int socket_fd, unsigned char dev_address, unsigned char channel_num, float point_val, unsigned char Point_num, unsigned char type);

//The following RX Functions used on the pseudo_SDAQ Simulator 
				/*pseudo_SDAQ -> Master Functions*/
int p_debug_data(int socket_fd, unsigned char dev_address, unsigned short ref_time, unsigned short dev_time);
int p_DeviceID_and_status(int socket_fd, unsigned char dev_address, unsigned int SN, unsigned char status);
int p_DeviceInfo(int socket_fd, unsigned char dev_address, unsigned char amount_of_channel);
int p_calibration_date(int socket_fd, unsigned char dev_address, unsigned char channel, sdaq_calibration_date *ch_cal_date);
int p_calibration_points_data(int socket_fd, unsigned char dev_address, unsigned char channel, sdaq_calibration_points_data *ch_cal_point_data);
int p_measure(int socket_fd, unsigned char dev_address, unsigned char channel, unsigned char state, float value, unsigned short timestamp);
int p_measure_raw(int socket_fd, unsigned char dev_address, unsigned char channel, unsigned char state, float value, unsigned short timestamp);
