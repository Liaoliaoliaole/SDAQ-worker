#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <linux/can.h>
#include <linux/can/raw.h>

//Include SDAQ Driver header
#include "sdaq_drv.h"
//Include Functions implementation files
#include "Modes.h"


//global variables


//application functions
void print_usage(char *prog_name);


int main(int argc, char *argv[])
{
	//Variables for Socket CAN
	struct timeval tv;
	struct ifreq ifr;
	struct sockaddr_can addr;	
	struct can_filter RX_filter;
	sdaq_can_id *can_filter_enc;
	int socket_num;
	//Variables for SDAQ_dev
	unsigned char dev_addr=0;
	unsigned int serial_number;
	
	if(argc <= 2)
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
	
	//Link inerface name to socket
	strcpy(ifr.ifr_name, argv[1]); // get name from main arguments
	if(ioctl(socket_num, SIOCGIFINDEX, &ifr))
	{
		printf("CANBUS interface name does not exist\n");
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
	tv.tv_sec = 25;
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
	//modes with device address requirement
	if(!strcmp(argv[2],"discover"))
	{
		//Discovery(socket_num);
	}
	else if(!strcmp(argv[2],"autoconf"))
	{
		//Autoconf(socket_num);
	}
	else //modes with device address requirement
	{
		//Sanity check of the device address arguments
		if(argv[3]==NULL)
		{
			printf("Address missing\n");
			exit(1);
		}
		dev_addr = atoi(argv[3]); // convert argument string to number
		if(dev_addr<1||dev_addr>=Parking_address)
		{
			printf("Device address: Out of range or invalid\n");
			exit(1);
		}
		
		//Scan for the rest of the modes
		if(!strcmp(argv[2],"address"))
		{
			if(argv[4]==NULL)
			{
				printf("SDAQ's Serial number is missing\n");
				exit(1);
			}
			serial_number = atoi(argv[4]); // convert argument string to number
			if(!serial_number)
			{
				printf("Serial number is invalid\n");
				exit(1);
			}
			//Change_address(socket_num,serial_number,dev_addr);
		}
		else if(!strcmp(argv[2],"info"))
		{
			//Dev_info(socket_num,dev_addr);
		}
		else if(!strcmp(argv[2],"measure"))
		{
			Measure(socket_num,dev_addr);
		}
		else if(!strcmp(argv[2],"log"))
		{
			//Logging(socket_num,dev_addr);
		}
		else
		{
			printf("Unknown mode argument\n");
		}
	}
	return 0;
}

void print_usage(char *prog_name)
{
	/*
	const char manual[] = {
		"commands that can be entered at runtime:\n"
		" q<ENTER>        - quit\n"
		" b<ENTER>        - toggle binary / HEX-ASCII output\n"
		" B<ENTER>        - toggle binary with gap / HEX-ASCII output (exceeds 80 chars!)\n"
		" c<ENTER>        - toggle color mode\n"
		" #<ENTER>        - notch currently marked/changed bits (can be used repeatedly)\n"
		" *<ENTER>        - clear notched marked\n"
		" rMYNAME<ENTER>  - read settings file (filter/notch)\n"
		" wMYNAME<ENTER>  - write settings file (filter/notch)\n"
		" +FILTER<ENTER>  - add CAN-IDs to sniff\n"
		" -FILTER<ENTER>  - remove CAN-IDs to sniff\n"
		"\n"
		"FILTER can be a single CAN-ID or a CAN-ID/Bitmask:\n"
		" +1F5<ENTER>     - add CAN-ID 0x1F5\n"
		" -42E<ENTER>     - remove CAN-ID 0x42E\n"
		" -42E7FF<ENTER>  - remove CAN-ID 0x42E (using Bitmask)\n"
		" -500700<ENTER>  - remove CAN-IDs 0x500 - 0x5FF\n"
		" +400600<ENTER>  - add CAN-IDs 0x400 - 0x5FF\n"
		" +000000<ENTER>  - add all CAN-IDs\n"
		" -000000<ENTER>  - remove all CAN-IDs\n"
		"\n"
		"if (id & filter) == (sniff-id & filter) the action (+/-) is performed,\n"
		"which is quite easy when the filter is 000\n"
		"\n"
	};
	*/
	printf("Usage: %s CAN-IF MODE [ADDRESS] [Options]\n",prog_name);
	return;
}


