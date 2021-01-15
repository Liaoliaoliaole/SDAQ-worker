/*
File: Modes.h Declaration for Mode functions
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

// enumerator for time_stamp_mode
enum time_stamp_mode{
	relative,
	absolute,
	absolute_with_date
};
// struct that contains the user's options
typedef struct option_flags{
	unsigned char timestamp_mode;
	char *CANif_name;
	char *timestamp_format;
	char *info_file;
	char *ext_com;
	unsigned silent : 1;
	unsigned formatted_output :1;
	unsigned verify : 1;
	unsigned resize : 1;
	unsigned int timeout;
}opt_flags;

/*The following two type defs structs used in info.c file and SDAQ_xml.c*/
/*	Struct SDAQ_information_and_calibration_data
		used in mode info and calibration.
		Contains:
			 internal struct SDAQ info.
			 A List with calibration points data (aka sdaq_calibration_data) for each channel
			 A list with dates and amount of data (aka sdaq_calibration_date) for each channel
*/
typedef struct SDAQ_information_and_calibration_data{
	struct SDAQ_info{
		unsigned int serial_number;
		const char *dev_type;
		unsigned char firm_rev;
		unsigned char hw_rev;
		unsigned char num_of_ch;
		unsigned char sample_rate;
		unsigned char max_cal_point;
	}SDAQ_info;
	struct GSList *Calibration_date_list; //list with data type date_list_data_of_node
	struct GSList **Cal_points_data_lists;//array of lists with data type sdaq_calibration_points_data
}SDAQ_info_cal_data;

//struct used as container type for the data of the Calibration_date_list
typedef struct calibration_date{
	unsigned char ch_num;
	unsigned char year;//after 12000
	unsigned char month;// 1 to 12
	unsigned char day;//1 to 31
	unsigned char period;//Calibration interval in months
	unsigned char amount_of_points;
	unsigned char cal_unit;
}date_list_data_of_node;

/*All the functions return EXIT_SUCCESS at success and EXIT_FAILURE on failure*/

//Declaration of function for Discovery mode. Implemented at Discover_and_autoconfig.c
int Discover(int socket_num, opt_flags *usr_flag);

//Declaration of function for Autoconf mode. Implemented at Discover_and_autoconfig.c
int Autoconfig(int socket_num, opt_flags *usr_flag);

//Declaration of function for Address mode. Implemented at SDAQ_worker.c
int Change_address(int socket_num, unsigned int serial_number, unsigned char new_address, opt_flags *usr_flag);

//Declaration of function for Measuring mode. Implemented at Measure.c
int Measure(int socket_num,unsigned char dev_addr, opt_flags *usr_flag);

//Declaration of function for Logging mode. Implemented at Logging.c
int Logging(int socket_num,unsigned char dev_addr, opt_flags *usr_flag);

//Declaration of function for GetInfo mode. Implemented at Dev_info.c
int getinfo(int socket_num,unsigned char dev_addr, opt_flags *usr_flag);

//Declaration of function for SetInfo mode. Implemented at Dev_info.c
int setinfo(int socket_num, unsigned char dev_addr, opt_flags *usr_flag);
