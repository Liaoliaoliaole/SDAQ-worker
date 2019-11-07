
#define PROTOCOL_ID 0x35

extern const char *unit_str[]; 
extern const char *dev_type_str[];
extern const char *dev_status_str[][8]; 
extern const unsigned char Parking_address;

// enumerator for Status byte
enum status_byte
{
	Run_Standby,
	In_sync,
	Error,
	Bootloader = 7
};

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
	Configure_Additional_data = 12,
	Measurement_value = 0x84, 
	Device_status = 0x86, 
	Device_info = 0x88,
	Calibration_Date = 0x89,
	Calibration_Point_Data = 0x8a,
	Uncalibrated_meas = 0x8b,
	//Bootloader_Reply = 0xa0,
	//Page_Buffer_Data = 0xa1,
	Sync_Info = 0xc0
};

#pragma pack(push, 1)//use pragma pack() to pack the following structs to 1 byte size (aka no zero padding)

/* SDAQ's CAN identifier encoder/decoder */
typedef struct SDAQ_Identifier_Encoder_Decoder
{
	unsigned channel_num : 6;
	unsigned device_addr : 6;
	unsigned payload_type: 8;
	unsigned protocol_id : 6;
	unsigned priority : 3;
	unsigned flags : 3;//EFF/RTR/ERR flags
}sdaq_can_id; 

/* SDAQ's CAN measurement message decoder */
typedef struct SDAQ_Measurement_Decoder
{
	float meas;
	unsigned char unit;
	unsigned char status;
	unsigned short timestamp;
}sdaq_meas; 

/* SDAQ's CAN Device_ID/Status message decoder */
typedef struct SDAQ_Status_Decoder
{
	unsigned int  dev_sn;
	unsigned char status;
	unsigned char dev_type;
}sdaq_status; 

/* SDAQ's CAN Device_info message decoder */
typedef struct SDAQ_Info_Decoder
{
	unsigned char dev_type;
	unsigned char firm_rev;
	unsigned char hw_rev;
	unsigned char num_of_ch;
	unsigned char sample_rate;
}sdaq_info; 

//The following RX Decoders used on the pseudo_SDAQ Simulator 
				/*RX Decoders*/
/* SDAQ's CAN Set Device Address message decoder */
typedef struct pSDAQ_Set_new_address
{
	unsigned int  dev_sn;
	unsigned char new_address;
}sdaq_set_new_addr; 

#pragma pack(pop)//Disable packing 

				/*TX Functions*/
/*All the functions return 0 in success and 1 on failure */
//Request start of measure from the SDAQ device. For all dev_addr=0
int Start(int socket_fd,unsigned char dev_address);
//Request stop of measure from the SDAQ device. For all dev_addr=0
int Stop(int socket_fd,unsigned char dev_address);
//Synchronize the SDAQ devices. Requested by broadcast only.
int Sync(int socket_fd, short time_seed);
//Control Configure Additional data. If Device is in measure will transmit raw measurement message
int Raw_meas(int socket_fd,unsigned char dev_address,const unsigned char Config);
//request change of device address with the specific serial number.
int SetDeviceAddress(int socket_fd,unsigned int dev_SN, unsigned char new_dev_address);
//request device info. Device answer with 3 messages: Device ID/status, Device Info and Calibration Date. 
int QueryDeviceInfo(int socket_fd,unsigned char dev_address);

//int WriteCalibrationDate(int socket_fd,unsigned char dev_address,time_t valid_until,unsigned char NumOfPoints);
//int WriteCalibrationPoints();

/*
0x84 Measurement value

0x86 Device ID/status

0x88 Device Info

0x89 Calibration Date

0x8a Calibration Point Data

0x8b Uncalibrated meas. value

0xc0 Sync Info

*/

//The following RX Functions used on the pseudo_SDAQ Simulator 
				/*RX Functions*/
int p_DeviceID_and_status(int socket_fd,unsigned char dev_address, unsigned int SN, unsigned char status);

