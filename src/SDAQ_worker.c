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
#include "SDAQ_drv.h"
//Include Functions implementation header
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
	
	//Link interface name to socket
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
	//modes with device address requirement
	if(!strcmp(argv[2],"discover"))
	{
		Discover(socket_num, 0);
	}
	else if(!strcmp(argv[2],"autoconf"))
	{
		Discover(socket_num, 1);
	}
	else //modes with device address requirement
	{
		//Sanity check of the device address arguments
		if(argv[3]==NULL)
		{
			printf("Address argument is missing\n");
			exit(1);
		}
		if(strcmp(argv[3],"parking")) //check address argument for parking 
		{
			dev_addr = atoi(argv[3]); // convert argument string to number
			if(dev_addr<1||dev_addr>=Parking_address)
			{
				printf("Device address: Out of range or invalid\n");
				exit(1);
			}
		}
		else
		{
			dev_addr = Parking_address;
			if(strcmp(argv[2],"address"))
			{
				printf("Device address: Out of range or invalid\n");
				exit(1);
			}	
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
			SetDeviceAddress(socket_num,serial_number,dev_addr);
		}
		else if(!strcmp(argv[2],"info"))
		{
			Dev_info(socket_num,dev_addr);
		}
		else if(!strcmp(argv[2],"measure"))
		{
			Measure(socket_num,dev_addr);
		}
		else if(!strcmp(argv[2],"logging"))
		{
			Logging(socket_num,dev_addr);
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
	const char manual[] = {
		"CAN-IF: The name of the CANBUS adapter\n\n"
		"MODE: discover: Discovering the connected SDAQs.\n"
		"      autoconf: Like discover but configure the devices on Park with a valid address.\n"
		"       address: Change the address of a SDAQ. Needed arguments: Serial Number and new address\n"
		"                  Call it as: address [number of new address] [Serial number of the SDAQ]\n"
		"          info: Get all the available information of a SDAQ device. Needed arguments: Address of the SDAQ.\n"
		"                  Call it as: info [SDAQ address]\n"
		"       measure: Get the measurement, status and info of a SDAQ device. Needed arguments: Address of the SDAQ.\n"
		"                  Call it as: measure [SDAQ address]\n"
		"       logging: Get and log to file the measurement of a SDAQ device. Needed arguments: Address of the SDAQ, Path of the logging file.\n"
		"                  Call it as: measure [SDAQ address] [Path of the logging file]\n"
		"\n"
	};
	printf("\nUsage: %s [Options] CAN-IF MODE [ADDRESS] \n\n%s",prog_name,manual);
	return;
}


