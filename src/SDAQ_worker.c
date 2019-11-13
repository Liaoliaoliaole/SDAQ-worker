/*   
Program: SDAQ_worker. A controlling software for SDAQ-CAN Devices.
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
#define VERSION "0.8 beta" /*Release Version of SDAQ_worker*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>

#include <linux/if_link.h>
#include <linux/can.h>
#include <linux/can/raw.h>

//Include SDAQ Driver header
#include "SDAQ_drv.h"
//Include Functions implementation header
#include "Modes.h"


//global variables

//application functions
void list_CANIF();//print a list of the available can interfaces
void print_usage(char *prog_name);//print the usage manual

int main(int argc, char *argv[])
{
	//Option parsing variables
	int c;
	opt_flags usr_opt = {.timestamp_mode=relative,
						 .timestamp_format=NULL,
						 .info_file=NULL,
						 .verify=0,
						 .silent=0,
						 .timeout = 3 //second
						}; 
	//Variables for Socket CAN
	struct timeval tv;
	struct ifreq ifr;
	struct sockaddr_can addr;	
	struct can_filter RX_filter;
	sdaq_can_id *can_filter_enc;
	int socket_num;
	//Variables for SDAQ_dev
	unsigned char dev_addr = 0;
	unsigned int serial_number;
	
	if(argc == 1)
	{
		print_usage(argv[0]);
		exit(1);
	}
	
	opterr = 1;
	while ((c = getopt (argc, argv, "hVvslt:S:T:f::")) != -1)
	{
		switch (c)
		{
			case 'h'://help
				print_usage(argv[0]);
				exit(EXIT_SUCCESS);
			case 'V'://Version
				printf(VERSION"\n");
				exit(EXIT_SUCCESS);
			case 'l'://List of CAN-IF
				list_CANIF();
				exit(EXIT_SUCCESS);
			case 's'://silent
				usr_opt.silent = 1;
				break;
			case 'f'://file, optional argument.   
				usr_opt.info_file = optarg ? optarg : "-"; //if no argument use stdio. 
				break;
			case 'v'://verify
				usr_opt.verify = 1;
				break;
			case 't'://timeout
				usr_opt.timeout = atoi(optarg);
				if(!usr_opt.timeout || usr_opt.timeout>20)
				{
					fprintf(stderr,"Timeout's argument is out of range (0 < Timeout < 20).\n");
					print_usage(argv[0]);
					exit(EXIT_FAILURE);
				}
				break;
			case 'S'://timestamp mode
				switch(optarg[0])
				{
					case 'A':
						usr_opt.timestamp_mode = absolute;
						break;
					case 'R':
						usr_opt.timestamp_mode = relative;
						break;
					case 'D':
						usr_opt.timestamp_mode = absolute_with_date;
						break;
					default :
						fprintf(stderr,"Unknown Timestamp's mode\n");
						print_usage(argv[0]);
						exit(EXIT_FAILURE);
				}
				break;
			case 'T':
				// to be sanitized 
				//usr_opt.timestamp_format = optarg;
				printf("Not implemented\n");
				printf("-T argument = \"%s\"\n",optarg);
				break;
			case '?':
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
				break;
		}
	}
	if(argv[optind] == NULL || argv[1] == NULL || argc <=2) 
	{
		printf("!!! CAN-IF and/or MODE Field are Missing !!!\n"); 
		exit(EXIT_FAILURE);		
	}
	//CAN Socket Opening
	if((socket_num = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) 
	{
		perror("Error while opening socket");
		exit(EXIT_FAILURE);
	}
	//Link interface name to socket
	strcpy(ifr.ifr_name, argv[optind]); // get value from CAN-IF arguments
	if(ioctl(socket_num, SIOCGIFINDEX, &ifr))
	{
		perror("CAN-IF");
		exit(EXIT_FAILURE);
	}
	/*Filter for CAN messages	-- SocketCAN Filters act as: <received_can_id> & mask == can_id & mask*/
	//load filter's can_id member
	can_filter_enc = (sdaq_can_id *)&RX_filter.can_id;//Set encoder to filter.can_id
	memset(can_filter_enc, 0, sizeof(sdaq_can_id));
	can_filter_enc->flags = 4;//set the EFF
	can_filter_enc->protocol_id = PROTOCOL_ID; // Received Messages with protocol_id == PROTOCOL_ID
	can_filter_enc->payload_type = 0x80; //  Received Messages with payload_type & 0x80 == TRUE, aka Master <- SDAQ.
	//load filter's can_mask member
	can_filter_enc = (sdaq_can_id *)&RX_filter.can_mask; //Set encoder to filter.can_mask
	memset(can_filter_enc, 0, sizeof(sdaq_can_id));
	can_filter_enc->flags = 4;//Received only messages with extended ID (29bit)
	can_filter_enc->protocol_id = -1; // Protocol_id field marked for examination 
	can_filter_enc->payload_type = 0x80; // + The most significant bit of Payload_type field marked for examination.  	
	setsockopt(socket_num, SOL_CAN_RAW, CAN_RAW_FILTER, &RX_filter, sizeof(RX_filter));
	
	// Add timeout option to the CAN Socket
	tv.tv_sec = 20;//interval time that a SDAQ send a Status/ID frame.
	tv.tv_usec = 0;
	setsockopt(socket_num, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
	
	//Bind CAN Socket to address
	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if(bind(socket_num, (struct sockaddr *)&addr, sizeof(addr)) < 0) 
	{
		perror("Error in socket bind");
		exit(EXIT_FAILURE);
	}	 
	
	/*Scan Mode argument*/
	//Modes with device address requirement
	if(!strcmp(argv[optind+1],"discover"))
	{
		Discover(socket_num, &usr_opt);
	}
	else if(!strcmp(argv[optind+1],"autoconfig"))
	{
		Autoconfig(socket_num, &usr_opt);
	}
	else //modes with device address requirement
	{
		//Sanity check of the device address arguments
		if(argv[optind+2]==NULL)
		{
			printf("Address argument is missing\n");
			exit(EXIT_FAILURE);
		}
		if(strcmp(argv[optind+2],"parking")) //check address argument for not be string "parking"
		{
			dev_addr = atoi(argv[optind+2]); // convert argument string to number
			if(dev_addr<1||dev_addr>=Parking_address)
			{
				printf("Device address: Out of range or invalid\n");
				exit(EXIT_FAILURE);
			}
		}
		else
		{
			dev_addr = Parking_address;
			if(strcmp(argv[optind+1],"setaddress"))// argument Parking allowed only for "setaddress" mode
			{
				printf("Device address: Out of range or invalid\n");
				exit(EXIT_FAILURE);
			}	
		}
		//Scan for the rest of the modes
		if(!strcmp(argv[optind+1],"setaddress"))
		{
			if(argv[optind+3]==NULL)
			{
				printf("SDAQ's Serial number is missing\n");
				exit(EXIT_FAILURE);
			}
			serial_number = atoi(argv[optind+3]); // convert argument string to number
			if(!serial_number)
			{
				printf("Serial number is invalid\n");
				exit(EXIT_FAILURE);
			}
			Change_address(socket_num,serial_number, dev_addr, &usr_opt);
		}
		else if(!strcmp(argv[optind+1],"info"))
		{
			info(socket_num, dev_addr, &usr_opt);
		}
		else if(!strcmp(argv[optind+1],"measure"))
		{
			Measure(socket_num, dev_addr, &usr_opt);
		}
		else if(!strcmp(argv[optind+1],"logging"))
		{
			Logging(socket_num, dev_addr, &usr_opt);
		}
		else
			printf("Unknown mode argument\n");
	}
	close(socket_num);
	return EXIT_SUCCESS;
}

int Change_address(int socket_num, unsigned int serial_number, unsigned char new_address, opt_flags *usr_flag)
{
	unsigned char amount_of_tests=10;
	//CAN Socket and SDAQ related variables
	struct can_frame frame_rx;
	int RX_bytes;
	sdaq_can_id *id_dec = (sdaq_can_id *)&(frame_rx.can_id);
	sdaq_status *status_dec = (sdaq_status *)&(frame_rx.data);
	SetDeviceAddress(socket_num, serial_number, new_address);
	if(usr_flag->verify)
	{
		printf("Check address of SDAQ with S/N:%d\n",serial_number);
		QueryDeviceInfo(socket_num,new_address);
		do{
			putchar('.');
			fflush(stdout);
			RX_bytes=read(socket_num, &frame_rx, sizeof(frame_rx));
			if(RX_bytes==sizeof(frame_rx))
			{
				if(id_dec->device_addr==new_address && status_dec->dev_sn == serial_number)
					break;	
			}
			else
			{
				printf("Timeout\n");
				return EXIT_FAILURE;
			}
			amount_of_tests--;
		}while(amount_of_tests);
		if(amount_of_tests)
		{
			if(!usr_flag->silent)
				printf("\nSUCCESS\n");
		}
		else
		{
			printf("\nError: SDAQ not answering!!!!\n");
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

void list_CANIF()
{
	struct ifaddrs *ifaddr, *ifa;
	
	if (getifaddrs(&ifaddr) == -1) 
	{
		perror("getifaddrs");
		exit(EXIT_FAILURE);
	}
	/*Scan through the list ifaddr.
	Print ifa_name field of every node where the ifa_addr == NULL -- Possible make it better in future*/
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) 
	{
		if (ifa->ifa_addr == NULL)
			printf("%s\n",ifa->ifa_name);
	}
	freeifaddrs(ifaddr);
}

void print_usage(char *prog_name)
{
	const char preamp[] = {
	"\tProgram: SDAQ_worker  Copyright (C) 12019-12020  Sam Harry Tzavaras\n"
    "\tThis program comes with ABSOLUTELY NO WARRANTY; for details see LICENSE.\n"
    "\tThis is free software, and you are welcome to redistribute it\n"
    "\tunder certain conditions; for details see LICENSE.\n\n"	
	};
	const char manual[] = {
		"CAN-IF: The name of the CAN-Bus adapter\n\n"
		"MODE:\n"
		"      discover: Discovering the connected SDAQs.\n\n"
		"    autoconfig: Set valid address to all Parked SDAQs.\n\n"
		"    setaddress: Change the address of a SDAQ.\n"
		"                (Usage: SDAQ_worker CAN-IF setaddress 'new_address' 'Serial_number_of_SDAQ')\n"
		"          info: Get all the available information of a SDAQ device.\n"
		"                (Usage: SDAQ_worker CAN-IF info 'SDAQ_address')\n"
		"       measure: Get the measurements, status and info of a SDAQ device.\n"
		"                (Usage: SDAQ_worker CAN-IF measure 'SDAQ_address')\n"
		"       logging: Get and log the measurement of a SDAQ device to a file.\n"
		"                (Usage: SDAQ_worker CAN-IF logging 'SDAQ_address' 'Path/to/the/logging_directory')\n\n"
		"ADDRESS: A valid SDAQ address. Resolution 1..62 (also 'Parking' but works only for Mode 'setaddress'\n\n"
		"Options:\n"
		"           -h : Print help.\n"
		"           -V : Version.\n"
		"           -s : Silent mode.\n"
		"           -v : Address Verification. Used with mode 'setaddress'.\n"
		"           -l : Print a list of the available CAN-IF.\n" 
		"           -f : Write SDAQ info. Used with mode 'info'\n" 
		"  -t <Timeout>: Discover Timeout (sec). (0 < Timeout < 20) default: 1 Sec\n"
		"  -S <Mode>   : Timestamp mode. (A)bsolute/(R)elative/(D)ate.\n"
		"  -T <format> : Timestamp format, works with -S Date.\n"
		"\n"
	};
	printf("%s\nUsage: %s CAN-IF MODE [ADDRESS] [SERIAL NUMBER] [LOGGING DIRECTOR] [Options]\n\n%s",preamp, prog_name, manual);
	return;
}


