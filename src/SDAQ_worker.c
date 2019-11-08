#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/can.h>
#include <linux/can/raw.h>

//Include SDAQ Driver header
#include "SDAQ_drv.h"
//Include Functions implementation header
#include "Modes.h"


//global variables

//application functions
void print_usage(char *prog_name);

int main(int argc, char *argv[])
{
	//Option parsing variables
	int c;
	opt_flags usr_opt = {.timestamp_mode=relative,.timestamp_format=NULL,.silent=0,.timeout=1};
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
	while ((c = getopt (argc, argv, "hst:S:T:")) != -1)
	{
		switch (c)
		{
			case 'h'://help
				print_usage(argv[0]);
				exit(1);
			case 's'://silent
				usr_opt.silent = 1;
				break;
			case 't'://timeout
				usr_opt.timeout = atoi(optarg);
				if(!usr_opt.timeout || usr_opt.timeout>20)
				{
					fprintf(stderr,"Timeout's argument is out of range (0 < Timeout < 20).\n");
					print_usage(argv[0]);
					exit(1);
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
						exit(1);
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
				exit(1);
				break;
		}
	}
	if(argv[optind] == NULL) 
	{
		print_usage(argv[0]);
		exit(1);		
	}
	//CAN Socket Opening
	if((socket_num = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) 
	{
		perror("Error while opening socket");
		exit(1);
	}
	
	//Link interface name to socket
	strcpy(ifr.ifr_name, argv[optind]); // get name from main arguments
	if(ioctl(socket_num, SIOCGIFINDEX, &ifr))
	{
		perror("CAN-IF");
		exit(1);
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
	
	/*
	//Disable Loopback
	const int disable_loopback = 0;
	//setsockopt(socket_num, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &disable_loopback, sizeof(disable_loopback)); 
	*/
	
	// Add timeout option to the CAN Socket
	tv.tv_sec = 20;
	tv.tv_usec = 0;
	setsockopt(socket_num, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
	
	//Bind CAN Socket to address
	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if(bind(socket_num, (struct sockaddr *)&addr, sizeof(addr)) < 0) 
	{
		perror("Error in socket bind");
		exit(1);
	}	 
	
	/*Scan Mode argument*/
	//Modes with device address requirement
	if(!strcmp(argv[optind+1],"discover"))
	{
		Discover(socket_num, usr_opt);
	}
	else if(!strcmp(argv[optind+1],"autoconfig"))
	{
		Autoconfig(socket_num, usr_opt);
	}
	else //modes with device address requirement
	{
		//Sanity check of the device address arguments
		if(argv[optind+2]==NULL)
		{
			printf("Address argument is missing\n");
			exit(1);
		}
		if(strcmp(argv[optind+2],"parking")) //check address argument for parking 
		{
			dev_addr = atoi(argv[optind+2]); // convert argument string to number
			if(dev_addr<1||dev_addr>=Parking_address)
			{
				printf("Device address: Out of range or invalid\n");
				exit(1);
			}
		}
		else
		{
			dev_addr = Parking_address;
			if(strcmp(argv[optind+1],"address"))
			{
				printf("Device address: Out of range or invalid\n");
				exit(1);
			}	
		}
		//Scan for the rest of the modes
		if(!strcmp(argv[optind+1],"address"))
		{
			if(argv[optind+3]==NULL)
			{
				printf("SDAQ's Serial number is missing\n");
				exit(1);
			}
			serial_number = atoi(argv[optind+3]); // convert argument string to number
			if(!serial_number)
			{
				printf("Serial number is invalid\n");
				exit(1);
			}
			//Change_address(socket_num,serial_number, dev_addr, usr_opt);
			SetDeviceAddress(socket_num, serial_number, dev_addr);
		}
		else if(!strcmp(argv[optind+1],"info"))
		{
			Dev_info(socket_num, dev_addr, usr_opt);
		}
		else if(!strcmp(argv[optind+1],"measure"))
		{
			Measure(socket_num, dev_addr, usr_opt);
		}
		else if(!strcmp(argv[optind+1],"logging"))
		{
			Logging(socket_num, dev_addr, usr_opt);
		}
		else
			printf("Unknown mode argument\n");
	}
	close(socket_num);
	return 0;
}

void print_usage(char *prog_name)
{
	const char manual[] = {
		"CAN-IF: The name of the CAN-Bus adapter\n\n"
		"MODE\n"
		"      discover: Discovering the connected SDAQs.\n\n"
		"    autoconfig: Set valid address to all Parked SDAQs.\n\n"
		"       address: Change the address of a SDAQ.\n"
		"                (Call it as: address 'new_SDAQ_address' 'Serial_number_of_SDAQ')\n"
		"          info: Get all the available information of a SDAQ device.\n"
		"                (Call it as: info 'SDAQ_address')\n"
		"       measure: Get the measurement, status and info of a SDAQ device.\n"
		"                (Call it as: measure 'SDAQ_address')\n"
		"       logging: Get and log the measurement of a SDAQ device to a file.\n"
		"                (Call it as: logging 'SDAQ_address' 'Path/to/the/logging_directory')\n"
		"Options\n"
		"           -h : Print help.\n"
		"           -s : Silent mode.\n"
		"  -t <Timeout>: Discover Timeout (sec). (0 < Timeout < 20) default: 1 Sec\n"
		"  -S <Mode>   : Timestamp mode. (A)bsolute/(R)elative/(D)ate.\n"
		"  -T <format> : Timestamp format, works with -S Date.\n"
		"\n"
	};
	printf("\nUsage: %s CAN-IF MODE [ADDRESS] [SERIAL NUMBER] [PATH TO LOGGING FILE] [Options]\n\n%s",prog_name,manual);
	return;
}


