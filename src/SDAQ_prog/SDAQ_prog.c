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
void print_usage(char *prog_name);//Print the usage manual

int main(int argc, char *argv[])
{
	char *iHEX_file_path = NULL;
	rom_data SDAQ_flash = ROM_DATA_INIT;
	//Option parsing variables
	int c, retval=EXIT_FAILURE;

	if(argc == 1)
	{
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	opterr = 1;
	while ((c = getopt (argc, argv, "hVlf:")) != -1)
	{
		switch (c)
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
			case 'f'://file
				iHEX_file_path = optarg;
				break;
			case '?':
				exit(EXIT_FAILURE);
		}
	}
	if(argv[optind] == NULL || argv[1] == NULL || argc <=2)
	{
		printf("!!! CAN-IF and/or MODE argument Missing !!!\n");
		exit(EXIT_FAILURE);
	}

	if(!strcmp(argv[optind+1],"download"))
	{
		printf("Not implemented\n");
	}
	else if(!strcmp(argv[optind+1],"upload"))
	{
		if(iHEX_file_path)
		{
			if((retval = iHEX_read_file(iHEX_file_path, &SDAQ_flash)))
				fputs(iHEX_strerror(retval), stderr);
		}
		else
			fprintf(stderr, "File path is undefined!!!\n");
	}
	else
		printf("Unknown mode argument!!!\n");

	return retval;
}

void print_usage(char *prog_name)
{
	const char preamp[] = {
	"\tProgram: SDAQ_prog  Copyright (C) 12019-12021  Sam Harry Tzavaras\n"
    "\tThis program comes with ABSOLUTELY NO WARRANTY; for details see LICENSE.\n"
    "\tThis is free software, and you are welcome to redistribute it\n"
    "\tunder certain conditions; for details see LICENSE.\n"
	};
	const char manual[] = {
		"CAN-IF: The name of the CAN-Bus adapter\n\n"
		"MODE:\n"
		"      download: Download a hex file from SDAQ.\n\n"
		"        upload: Upload a hex file to SDAQ.\n\n"
		"ADDRESS: A valid SDAQ address. Resolution 1..62 (also 'Parking' for Mode 'setaddress')\n\n"
		"Options:\n"
		"           -h : Print help.\n"
		"           -V : Version.\n"
		"           -l : Print a list of the available CAN-IFs.\n"
		"           -f : Path to iHEX file.\n"
		"\n"
	};
	printf("%s\nUsage: %s CAN-IF MODE ADDRESS [Options]\n\n%s",preamp, prog_name, manual);
	return;
}
