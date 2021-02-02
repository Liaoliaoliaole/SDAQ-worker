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

//Application functions
int SDAQ_prog(char *CAN_IF, unsigned char SDAQ_addr, rom_data *SDAQ_flash, _Bool Print_error);
void print_usage(char *prog_name);//Print the usage manual

int main(int argc, char *argv[])
{
	_Bool Silent = FALSE, fl_stdin = FALSE;
	char *iHEX_file_path = NULL, *CAN_if = NULL;
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
			printf("CAN-IF argument Missing\n");
		if(!argv[optind+1])
			printf("ADDRESS argument Missing\n");
		exit(EXIT_FAILURE);
	}
	CAN_if = argv[optind];
	SDAQ_addr = atoi(argv[optind+1]);
	if(!SDAQ_addr || SDAQ_addr>=Parking_address)
	{
		if(!Silent)
			printf("Address is out of range!!!\n");
		exit(EXIT_FAILURE);
	}
	iHEX_file_path = argv[optind+2];
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
		retval = SDAQ_prog(CAN_if, SDAQ_addr, &SDAQ_flash,!Silent);
	free_rom_data(&SDAQ_flash);
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

int SDAQ_prog(char *CAN_IF, unsigned char SDAQ_addr, rom_data *SDAQ_flash, _Bool Print_error)
{


	if(!CAN_IF || !SDAQ_flash || (!SDAQ_addr || SDAQ_addr>=Parking_address))
		return EXIT_FAILURE;
/*
	if(SDAQ_flash->cs)
		printf("CS = 0x%04x\n", *SDAQ_flash->cs);
	if(SDAQ_flash->ip)
		printf("IP = 0x%04x\n", *SDAQ_flash->ip);
	if(SDAQ_flash->iep)
		printf("IEP = 0x%08x\n", *SDAQ_flash->iep);
	g_list_foreach(SDAQ_flash->data_blks, print_data_blks, DATA_PRINT_OFF);
*/
	return EXIT_SUCCESS;
}