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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <ncurses.h>

#include <glib.h>
#include <gmodule.h>

#include <sys/time.h>
#include <signal.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include "SDAQ_drv.h"
#include "Modes.h"
#include "SDAQ_xml.h"

//Local Functions
//function for decode external command
int str_dec(char **arg, char *input_buff, const char *delim);
//function for construction of struct tm with calibration date of SDAQ
int date_to_tm(struct tm *output_date, char *input_buff);

int setinfo(int socket_num, unsigned char dev_addr, opt_flags *usr_flag)
{
	char *argv[10];
	unsigned char argc, dev_address, channel_num, period, NumOfPoints, Point_num, type, unit;
	float point_val;
	struct tm date;
	if(usr_flag->ext_com)
	{
		argc = str_dec(argv, usr_flag->ext_com, " ");
		if(argc == 7 || argc == 6)
		{
			dev_address = atoi(argv[1]);
			channel_num = atoi(argv[2]);
			if(!strcmp(argv[0], "WriteCalibrationDate"))
			{
				if(dev_address && dev_address<Parking_address)
				{
					date_to_tm(&date, argv[3]);
					period = atoi(argv[4]);
					NumOfPoints = atoi(argv[5]);
					unit = atoi(argv[6]);
					WriteCalibrationDate(socket_num, dev_address, channel_num, &date, period, NumOfPoints, unit);
					return 0;
				}
			}
			else if(!strcmp(argv[0], "WriteCalibrationPoint"))
			{
				if(dev_address && dev_address<Parking_address)
				{
					point_val = atof(argv[3]);
					Point_num = atoi(argv[4]);
					type = atoi(argv[5]);
					WriteCalibrationPoint(socket_num, dev_address, channel_num, point_val, Point_num, type);
					return 0;
				}
			}
		}
		printf("External command is Unknown\n");
		return 0;
	}

	if(usr_flag->info_file)
	{
		printf("info XML File is \"%s\"\n",usr_flag->info_file);
		return 0;
	}
	printf("Not Implemented!!!\n");
	return 0;
}

int str_dec(char **arg, char *input_buff, const char *delim)
{
	unsigned char i=0;
	arg[i] = strtok (input_buff, delim);
	while (arg[i] != NULL)
	{
		i++;
		arg[i] = strtok (NULL, delim);
	}
	return i;
}


//function for construction of struct tm with calibration date of SDAQ
int date_to_tm(struct tm *output_date, char *input_buff)
{
	char *date_argv[10];
	if(str_dec(date_argv, input_buff, "/")==3)
	{
		memset(output_date, 0, sizeof(struct tm));
		output_date->tm_year = atoi(date_argv[0]) - 1900;
		output_date->tm_mon = atoi(date_argv[1]) - 1;
		output_date->tm_mday = atoi(date_argv[2]);
		return 0;
	}
	return -1;
}

