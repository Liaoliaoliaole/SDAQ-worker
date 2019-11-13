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

// enumerator for time_stamp_mode
enum time_stamp_mode
{
	relative,
	absolute,
	absolute_with_date
};
// struct that contains the user's options 
typedef struct option_flags
{	
	unsigned char timestamp_mode;
	char *timestamp_format;
	char *info_file;
	unsigned silent : 1;
	unsigned verify : 1;
	unsigned int timeout;	
}opt_flags;

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

//Declaration of function for Info mode. Implemented at Dev_info.c
int info(int socket_num,unsigned char dev_addr, opt_flags *usr_flag);
