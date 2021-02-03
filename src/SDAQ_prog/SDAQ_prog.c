/*
Program: SDAQ_prog. A firmware downloading program for SDAQ-CAN Devices.
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

#define VERSION "0.1" /*Release Version of SDAQ_prog*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include "../SDAQ_drv.h"
#include "../CANif_discovery.h"
#include "iHEX.h"

//Global variables.
static volatile _Bool run = TRUE;

//Application functions
int SDAQ_prog(char *CAN_IF, unsigned char SDAQ_addr, rom_data *SDAQ_flash, _Bool Print_error);
void print_usage(char *prog_name);//Print the usage manual
//Handler function for quit signals
inline static void quit_signal_handler(int signum)
{
	run = FALSE;
}

int main(int argc, char *argv[])
{
	_Bool Silent = FALSE, fl_stdin = FALSE;
	char *iHEX_file_path = NULL, *CAN_if_name = NULL;
	unsigned char SDAQ_addr;
	GString *iHEX_file_mem;
	rom_data SDAQ_flash = {0};
	//Option parsing variables
	int c, retval=EXIT_FAILURE;

	if(argc == 1)
	{
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	opterr = 1;
	while((c = getopt(argc, argv, "hVlsi")) != -1)
	{
		switch(c)
		{
			case 'h'://Help
				print_usage(argv[0]);
				exit(EXIT_SUCCESS);
			case 'V'://Version
				printf(VERSION"\n");
				exit(EXIT_SUCCESS);
			case 'l'://List of CAN-IF
				CANif_discovery();
				exit(EXIT_SUCCESS);
			case 's'://Silent
				Silent = TRUE;
				break;
			case 'i'://Interpreter
				fl_stdin = TRUE;
				break;
			case '?':
				exit(EXIT_FAILURE);
		}
	}
	if((argc - optind) < 2)
	{
		if(!argv[optind])
			fprintf(stderr, "CAN-IF argument Missing\n");
		if(!argv[optind+1])
			fprintf(stderr, "ADDRESS argument Missing\n");
		exit(EXIT_FAILURE);
	}
	if(strlen((CAN_if_name = argv[optind])) >= IFNAMSIZ)
	{
		fprintf(stderr, "CAN-IF name too big (>=%d)\n", IFNAMSIZ);
		exit(EXIT_FAILURE);
	}
	SDAQ_addr = atoi(argv[optind+1]);
	if(!SDAQ_addr || SDAQ_addr>=Parking_address)
	{
		fprintf(stderr, "Address is out of range!!!\n");
		exit(EXIT_FAILURE);
	}
	iHEX_file_path = argv[optind+2];

	//Link signal SIGINT, SIGTERM and SIGPIPE to quit_signal_handler
	signal(SIGINT, quit_signal_handler);
	signal(SIGTERM, quit_signal_handler);
	signal(SIGPIPE, quit_signal_handler);

	if(fl_stdin)
	{
		if(!Silent)
			printf("Run on Interpreter mode. Enter the iHEX file in stdin and close it with EOF (Ctrl+D)\n");
		if(!(iHEX_file_mem = g_string_new(NULL)))
		{
			fprintf(stderr, "Memory Error!!!\n");
			exit(EXIT_FAILURE);
		}
		while((c = getchar()) != EOF)
			iHEX_file_mem = g_string_append_c(iHEX_file_mem, c);
		retval = iHEX_read(NULL, iHEX_file_mem->str, &SDAQ_flash, !Silent);
		g_string_free(iHEX_file_mem, TRUE);
	}
	else if(iHEX_file_path)
		retval = iHEX_read(iHEX_file_path, NULL, &SDAQ_flash, !Silent);
	else
		fprintf(stderr, "File path is undefined!!!\n");

	if(!retval)
		retval = SDAQ_prog(CAN_if_name, SDAQ_addr, &SDAQ_flash,!Silent);
	free_rom_data(&SDAQ_flash);
	//printf("Bye bye.\n");
	return retval;
}

void print_usage(char *prog_name)
{
	const char preamp[] = {
	"Program: SDAQ_prog  Copyright (C) 12019-12021  Sam Harry Tzavaras\n"
    "\tThis program comes with ABSOLUTELY NO WARRANTY; for details see LICENSE.\n"
    "\tThis is free software, and you are welcome to redistribute it\n"
    "\tunder certain conditions; for details see LICENSE file.\n"
	};
	const char manual[] = {
		"CAN-IF: The name of the CANBUS interface.\n\n"
		"ADDRESS: A valid SDAQ address (Resolution:1..62).\n\n"
		"Options:\n"
		"           -h : Print help.\n"
		"           -V : Version.\n"
		"           -s : Silent mode.\n"
		"           -l : Print a list of the available CAN-IFs.\n"
		"           -i : Interpreter mode.\n"
		"\n"
	};
	printf("%s\nUsage: %s [Options] CAN-IF ADDRESS [Path to ROM File]\n\n%s",preamp, prog_name, manual);
	return;
}

int SDAQ_prog(char *CAN_IF_name, unsigned char SDAQ_addr, rom_data *SDAQ_flash, _Bool Print_error)
{
	//Variables for Socket CAN
	struct timeval tv = {0};
	struct ifreq ifr = {0};
	struct sockaddr_can addr = {0};
	struct can_filter RX_filter = {0};
	struct can_frame frame_rx;
	sdaq_can_id *sdaq_id_dec;
	int CAN_socket_num, RX_bytes, retval = EXIT_SUCCESS;

	//Chech arguments for invalid entry.
	if(!CAN_IF_name || !SDAQ_flash || (!SDAQ_addr || SDAQ_addr>=Parking_address))
		return EXIT_FAILURE;
	//CAN Socket Opening
	if((CAN_socket_num = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
	{
		perror("Error while opening socket");
		return EXIT_FAILURE;
	}
	//Link interface name to socket
	strcpy(ifr.ifr_name, CAN_IF_name); // get value from CAN-IF arguments
	if(ioctl(CAN_socket_num, SIOCGIFINDEX, &ifr))
	{
		perror("CAN-IF");
		return EXIT_FAILURE;
	}
	/*Filter for CAN messages	-- SocketCAN Filters act as: <received_can_id> & mask == can_id & mask*/
	//load filter's can_id member
	sdaq_id_dec = (sdaq_can_id *)&RX_filter.can_id;//Set encoder to filter.can_id
	memset(sdaq_id_dec, 0, sizeof(sdaq_can_id));
	sdaq_id_dec->flags = 4;//set the EFF
	sdaq_id_dec->protocol_id = PROTOCOL_ID; // Received Messages with protocol_id == PROTOCOL_ID
	sdaq_id_dec->payload_type = 0x80; //  Received Messages with payload_type & 0x80 == TRUE, aka Master <- SDAQ.
	sdaq_id_dec->device_addr = SDAQ_addr; // Receive only messages from SDAQ with address == SDAQ_addr.
	//load filter's can_mask member
	sdaq_id_dec = (sdaq_can_id *)&RX_filter.can_mask; //Set encoder to filter.can_mask
	memset(sdaq_id_dec, 0, sizeof(sdaq_can_id));
	sdaq_id_dec->flags = 4;//Received only messages with extended ID (29bit)
	sdaq_id_dec->protocol_id = -1; // Protocol_id field marked for examination
	sdaq_id_dec->payload_type = 0x80; // + The most significant bit of Payload_type field marked for examination.
	sdaq_id_dec->device_addr = -1; // Mark device_addr field to be examined.
	setsockopt(CAN_socket_num, SOL_CAN_RAW, CAN_RAW_FILTER, &RX_filter, sizeof(RX_filter));

	// Add timeout option to the CAN Socket
	tv.tv_sec = 2;//interval for timeout.
	tv.tv_usec = 0;
	setsockopt(CAN_socket_num, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

	//Bind CAN Socket to address
	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if(bind(CAN_socket_num, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("Error in socket bind");
		return EXIT_FAILURE;
	}

	//SDAQ_prog's FSM
	SDAQ_goto(CAN_socket_num, SDAQ_addr, bootloader);
	while(run)
	{
		RX_bytes=read(CAN_socket_num, &frame_rx, sizeof(frame_rx));
		if(RX_bytes==sizeof(frame_rx))
		{
			printf("Msg received!!!\n");
		}
	}
/*
	if(SDAQ_flash->cs)
		printf("CS = 0x%04x\n", *SDAQ_flash->cs);
	if(SDAQ_flash->ip)
		printf("IP = 0x%04x\n", *SDAQ_flash->ip);
	if(SDAQ_flash->iep)
		printf("IEP = 0x%08x\n", *SDAQ_flash->iep);
	g_list_foreach(SDAQ_flash->data_blks, print_data_blks, DATA_PRINT_OFF);
*/
	close(CAN_socket_num);//Close CAN_socket
	return retval;
}