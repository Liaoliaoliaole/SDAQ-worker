/*
File: SDAQ_drv.h, declaration of the function for the SDAQ driver.
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
#ifndef SDAQ_DRV_h
#define SDAQ_DRV_h

#define SDAQ_MAX_DEV_NUM 20 //Maximum number for SDAQ Device type
#define SDAQ_MAX_AMOUNT_OF_CHANNELS 16 //can be up to 63, from white paper.
#define MAX_AMOUNT_OF_POINTS 16
#define MAX_DATA_ON_POINT 6 //6 is the amount of values in each point (ref, read, offset, gain and coefficients).

#define PROTOCOL_ID 0x35

#define SDAQ_IMG_ADDR_OFFSET 0x400
#define SDAQ_IMG_PRE_PAGES 3
#define SDAQ_IMG_HEADER_WORD 0x18281827 // Value from white paper'
#define PAGE_SIZE 256 // Value from White paper.
#define PAGE_SECTIONS 32 // 32 = 256/8, 8 is maximum Payload size.
#define INP_MODE_MAX_COL 8

extern const char *unit_str[];
extern const char *dev_type_str[];
extern const char *dev_status_str[][8];
extern const char *channel_status_str[];
extern const char *type_of_point_str[];
extern const char *dev_input_mode_str[20][INP_MODE_MAX_COL];
extern const char *SDAQ_reg_status_str[];
extern const unsigned char Parking_address;
extern const unsigned char Broadcast;
extern const unsigned char Unit_code_base_region_size;

extern const char DEVID_indexer_str[];
extern const char DEVID_indexer_str_550X[];
extern const char REV_indexer_str[];
extern const unsigned int DEVID_indexer_str_550X_len;
extern const unsigned int DEVID_indexer_str_len;
extern const unsigned int REV_indexer_str_len;

// Enumerator of SDAQ registration status
#define SDAQ_INFO_PENDING_CHECK(reg_status) (reg_status && reg_status < SDAQ_registeration_max_state)
enum SDAQ_registeration_status{
	Registered = 1,
	Pending_Calibration_data,
	Pending_input_mode,
	Ready,
	SDAQ_registeration_max_state = Ready
};

// Enumerator for Device Status byte
enum status_byte{
	State = 0,
	In_sync = 1,
	Error = 2,
	Mode = 7
};

// Enumerator for Device Status byte
enum channel_status_byte{
	No_sensor = 0,
	Out_of_range = 1,
	Over_range = 2
};

// Enumerator for Calibration Point Data type byte
enum Calibration_Point_Data_type_byte{
	meas = 1,
	ref = 2,
	offset = 3,
	gain = 4,
	C2 = 5,
	C3 = 6
};
// Enumerator for payload_type
enum payload_type{
/* Messages payload_type. Master -> SDAQ */
	Synchronization_command = 1,
	Start_command = 2,
	Stop_command = 3,
	Set_dev_address = 6,
	Query_Dev_info = 7,
	Query_Calibration_Data = 8,
	Write_calibration_Date = 9,
	Write_calibration_Point_Data = 0x0A,
	Change_SDAQ_baudrate = 0x0B,
	Configure_Additional_data = 0x0C,
	Query_system_variables = 0x0D,
	Write_system_variable = 0x0E,
	//Bootloader related.
	goto_bootloader = 0x20,
	Erase_flash = 0x21,
	Write_to_page_buff = 0x22,
	Write_page_buff_to_flash = 0x23,
	Query_flash_data = 0x24,
	goto_application = 0x25,
/* Messages payload_type. SDAQ -> Master */
	Measurement_value = 0x84,
	Device_status = 0x86,
	Device_info = 0x88,
	Calibration_Date = 0x89,
	Calibration_Point_Data = 0x8a,
	Uncalibrated_meas = 0x8b,
	System_variable = 0x8d,
	//Bootloader related
	Bootloader_reply = 0xa0,
	Page_buff = 0xa1,
	//Debug message
	Sync_Info = 0xc0
};

enum SDAQ_goto_code{
	application = 0,
	bootloader = 1
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
	unsigned char max_cal_point;
}sdaq_info;

/* SDAQ's CAN Calibration_date message decoder */
typedef struct SDAQ_calibration_date_Decoder{
	unsigned char year;//after 12000
	unsigned char month;// 1 to 12
	unsigned char day;//1 to 31
	unsigned char period;//Calibration interval in months
	unsigned char amount_of_points;
	unsigned char cal_units;
}sdaq_calibration_date;

/* SDAQ's CAN Calibration_point_data message decoder */
typedef struct SDAQ_calibration_points_data_Decoder{
	float data_of_point;
	unsigned char type;
	unsigned char points_num;
}sdaq_calibration_points_data;

/* SDAQ's CAN System_variable message decoder */
typedef struct SDAQ_system_variable_data_Decoder{
	union variable_value_field{
		unsigned int as_uint32;
		float as_float;
	} var_val;
	unsigned char type;
}sdaq_sysvar;

	//--- SDAQ's Bootloader related messages---//
/* SDAQ firmware image header */
typedef struct SDAQ_img_header_str{
	unsigned int header_word;
	unsigned int start_addr;
	unsigned int end_addr;
	unsigned int crc;
	unsigned char reserved[PAGE_SIZE-sizeof(unsigned int)*4];
} SDAQ_img_header;

/* SDAQ's Bootloader response message decoder */
typedef struct SDAQ_bootloader_response_Decoder{
	unsigned char error_code;
	unsigned char command;
	unsigned char reserved[2];
	unsigned int IAP_ret;
}sdaq_bootloader_response;

/* SDAQ's Erase Flash memory message encoder */
typedef struct SDAQ_Erase_Flash_Encoder{
	unsigned int Start_addr;
	unsigned int End_addr;
}sdaq_flash_erase;

/* SDAQ's transfer page buffer to flash message encoder */
typedef struct SDAQ_Write_Buffer_Encoder{
	unsigned int addr;
}sdaq_transfer_buffer;

	//--- The following RX Decoders used on the pseudo_SDAQ Simulator ---//
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

//Decoder for the status byte field from "CAN Device_ID/Status" message
const char * status_byte_dec(unsigned char status_byte,unsigned char field);
//Decoder for the status byte field from "Measure" message
const char * Channel_status_byte_dec(unsigned char status_byte);

				/*Master -> SDAQ Functions*/
/*All the functions return 0 in success and 1 on failure */
//Request start of measure from the SDAQ device. For all: dev_addr=0
int Start(int socket_fd, unsigned char dev_address);
//Request stop of measure from the SDAQ device. For all: dev_addr=0
int Stop(int socket_fd, unsigned char dev_address);
//Synchronize the SDAQ devices. Requested by broadcast only.
int Sync(int socket_fd, unsigned short time_seed);
//Control Configure Additional data. If Device is in measure will transmit raw measurement message
int Req_Raw_meas(int socket_fd, unsigned char dev_address, const unsigned char Config);
//Request change of device address with the specific serial number.
int SetDeviceAddress(int socket_fd, unsigned int dev_SN, unsigned char new_dev_address);
//Request device info. Device answer with 3 messages types: Device ID/status, Device Info and Calibration Date for each channel
int QueryDeviceInfo(int socket_fd, unsigned char dev_address);
//Request calibration data. Device answer with 2 messages types: Calibration Date and Calibration Point Data for each channel
int QueryCalibrationData(int socket_fd, unsigned char dev_address, unsigned char channel);
//Request system variables. Device answer with all the system variables of the SDAQ.
int QuerySystemVariables(int socket_fd, unsigned char dev_address);
//Write the calibration date data of the channel 'channel_num' of the SDAQ with address 'dev_address'
int WriteCalibrationDate(int socket_fd, unsigned char dev_address, unsigned char channel_num, void *date_ptr, unsigned char period, unsigned char NumOfPoints, unsigned char unit);
//Write the calibration point data 'NumOfPoint' of the channel 'channel_num' of the SDAQ with address 'dev_address'
int WriteCalibrationPoint(int socket_fd, unsigned char dev_address, unsigned char channel_num, float point_val, unsigned char Point_num, unsigned char type);
		/*SDAQ's Bootloader related functions*/
//Set execution code of SDAQ's uC.
int SDAQ_goto(int socket_fd, unsigned char dev_address, _Bool code_reg_fl);
//Erase SDAQ's Flash memory region.
int SDAQ_erase_flash(int socket_fd, unsigned char dev_address, unsigned int first_addr, unsigned int last_addr);
/*
 * Function that write header of firmware image to SDAQ.
 * "ret_buff" is nullable.
 * if "ret_buff" is used, will be filled with PAGE_SIZE bytes of the header. So it's need to be at least PAGE_SIZE in size.
 */
int SDAQ_write_header(int socket_fd, unsigned char dev_address, unsigned int start_addr, unsigned int range, unsigned int crc, unsigned char *ret_buff);
//Write to page buffer. Data must be PAGE_SIZE bytes long.
int SDAQ_write_page_buff(int socket_fd, unsigned char dev_address, unsigned char *data);
//Transfer page buffer to Flash memory.
int SDAQ_Transfer_to_flash(int socket_fd, unsigned char dev_address, unsigned int addr);


//The following RX Functions used on the pseudo_SDAQ Simulator
				/*pseudo_SDAQ -> Master Functions*/
int p_debug_data(int socket_fd, unsigned char dev_address, unsigned short ref_time, unsigned short dev_time);
int p_DeviceID_and_status(int socket_fd, unsigned char dev_address, unsigned int SN, unsigned char status);
int p_DeviceInfo(int socket_fd, unsigned char dev_address, unsigned char amount_of_channel);
int p_calibration_date(int socket_fd, unsigned char dev_address, unsigned char channel, sdaq_calibration_date *ch_cal_date);
int p_calibration_points_data(int socket_fd, unsigned char dev_address, unsigned char channel, sdaq_calibration_points_data *ch_cal_point_data);
int p_measure(int socket_fd, unsigned char dev_address, unsigned char channel, unsigned char state, unsigned char unit, float value, unsigned short timestamp);
int p_measure_raw(int socket_fd, unsigned char dev_address, unsigned char channel, unsigned char state, float value, unsigned short timestamp);

#endif //SDAQ_DRV_h
