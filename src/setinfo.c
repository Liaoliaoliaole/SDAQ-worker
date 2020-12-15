/*
File: setinfo.c. Implementation of function for mode "setinfo"
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <ncurses.h>

#include <sys/time.h>
#include <signal.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include "info.h"//including -> "SDAQ_drv.h", "Modes.h"
#include "SDAQ_xml.h"

//Local Functions
//Function for decode external command
int str_dec(char **arg, char *input_buff, const char *delim);
//Function for construction of struct tm with calibration date of SDAQ
int date_to_tm(struct tm *output_date, char *input_buff);
//Function that correlate two SDAQ_info_cal_data (new and current config). Return EXIT_SUCCESS, if new related with the old; otherwise EXIT_FAILURE.
int corr_SDAQ_info_and_calibration_data(SDAQ_info_cal_data *cur_conf, SDAQ_info_cal_data *new_conf);

int setinfo(int socket_num, unsigned char dev_addr, opt_flags *usr_flag)
{
	char *argv[10];
	unsigned char argc, channel_num, period, NumOfPoints, Point_num, type, unit;
	float point_val;
	struct tm date;
	SDAQ_info_cal_data cur_conf={0}, new_conf={0};
	int retval;
	if(usr_flag->ext_com)
	{
		argc = str_dec(argv, usr_flag->ext_com, " ");
		if(argc == 6 || argc == 5)
		{
			channel_num = atoi(argv[1]);
			if(!strcmp(argv[0], "WriteCalibrationDate") && argc == 6)
			{
				if(dev_addr && dev_addr<Parking_address)
				{
					date_to_tm(&date, argv[2]);
					period = atoi(argv[3]);
					NumOfPoints = atoi(argv[4]);
					unit = atoi(argv[5]);
					WriteCalibrationDate(socket_num, dev_addr, channel_num, &date, period, NumOfPoints, unit);
					return EXIT_SUCCESS;
				}
			}
			else if(!strcmp(argv[0], "WriteCalibrationPoint") && argc == 5)
			{
				if(dev_addr && dev_addr<Parking_address)
				{
					point_val = atof(argv[2]);
					Point_num = atoi(argv[3]);
					type = atoi(argv[4]);
					WriteCalibrationPoint(socket_num, dev_addr, channel_num, point_val, Point_num, type);
					return EXIT_SUCCESS;
				}
			}
		}
		printf("External command is Unknown\n");
		return EXIT_FAILURE;
	}
	if(usr_flag->info_file)
	{
		if(XML_info_file_read_and_validate(usr_flag->info_file, &new_conf))
		{	//Free the list and the arrays of the new_conf
			g_slist_free_full((GSList *)(new_conf.Calibration_date_list), free_SDAQ_Date_node);
			if(new_conf.Cal_points_data_lists)
			{
				for(int i=0; i<new_conf.SDAQ_info.num_of_ch; i++)
					g_slist_free_full((GSList *)(new_conf.Cal_points_data_lists[i]), free_SDAQ_Date_node);
				free(new_conf.Cal_points_data_lists);
			}
			return EXIT_FAILURE;
		}
	}
	if(!(retval = get_SDAQ_info_and_calibration_data(socket_num, dev_addr, usr_flag->timeout, &cur_conf)))
	{
		if(usr_flag->info_file)
		{
			//printf("info XML File is \"%s\"\n",usr_flag->info_file);
			if(!corr_SDAQ_info_and_calibration_data(&cur_conf, &new_conf))
			{
				retval = set_SDAQ_info_and_calibration_data(socket_num, dev_addr, usr_flag->timeout, &new_conf);
				//Free the list and the arrays of the new_conf
				g_slist_free_full((GSList *)(new_conf.Calibration_date_list), free_SDAQ_Date_node);
				for(int i=0; i<new_conf.SDAQ_info.num_of_ch; i++)
					g_slist_free_full((GSList *)(new_conf.Cal_points_data_lists[i]), free_SDAQ_Date_node);
				free(new_conf.Cal_points_data_lists);
			}

		}
		else
		{
			printf("UI Not Implemented!!!\n");
		}
	}
	//Free the list and the arrays of the cur_conf
	g_slist_free_full((GSList *)(cur_conf.Calibration_date_list), free_SDAQ_Date_node);
	for(int i=0; i<cur_conf.SDAQ_info.num_of_ch; i++)
		g_slist_free_full((GSList *)(cur_conf.Cal_points_data_lists[i]), free_SDAQ_Date_node);
	free(cur_conf.Cal_points_data_lists);
	return retval;
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

int corr_SDAQ_info_and_calibration_data(SDAQ_info_cal_data *cur_conf, SDAQ_info_cal_data *new_conf)
{
	return EXIT_FAILURE;
}

//function that send the data from SDAQ_info_cal_data *str to SDAQ with address: dev_addr. Return: 0 on success or 1 on failure
int set_SDAQ_info_and_calibration_data(int socket_num, unsigned char dev_addr, unsigned int scanning_time, SDAQ_info_cal_data *SDAQ_config)
{
	return EXIT_FAILURE;
}

