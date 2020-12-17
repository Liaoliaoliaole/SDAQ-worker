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
#include <assert.h>
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
		{	
			free_SDAQ_info_cal_data(&new_conf);
			return EXIT_FAILURE;
		}
	}
	if(!(retval = get_SDAQ_info_and_calibration_data(socket_num, dev_addr, usr_flag->timeout, &cur_conf)))
	{
		if(usr_flag->info_file)
		{
			if(!corr_SDAQ_info_and_calibration_data(&cur_conf, &new_conf, INFO))
			{
				if(!(retval = set_SDAQ_info_and_calibration_data(socket_num, dev_addr, &new_conf)))
				{
					if(usr_flag->verify)
					{
						free_SDAQ_info_cal_data(&cur_conf);//Free the list and the arrays of the cur_conf
						if(!(retval = get_SDAQ_info_and_calibration_data(socket_num, dev_addr, usr_flag->timeout, &cur_conf)))
						{
							if(!(retval = corr_SDAQ_info_and_calibration_data(&cur_conf, &new_conf, DATE|POINTS)))
								printf("Verification completed successfully\n");
						}
					}
				}
			}
			free_SDAQ_info_cal_data(&new_conf);//Free the list and the arrays of the new_conf
		}
		else
		{
			printf("UI Not Implemented!!!\n");
		}
	}
	free_SDAQ_info_cal_data(&cur_conf);//Free the list and the arrays of the cur_conf
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

int corr_SDAQ_info_and_calibration_data(SDAQ_info_cal_data *cur_conf, SDAQ_info_cal_data *new_conf, unsigned char options)
{
	int retval = EXIT_FAILURE;

	if(!cur_conf || !new_conf)
		return EXIT_FAILURE;
	assert((options & (INFO|DATE|POINTS)));
	if(options & INFO)
	{
		if(cur_conf->SDAQ_info.serial_number == new_conf->SDAQ_info.serial_number &&
		   cur_conf->SDAQ_info.dev_type && new_conf->SDAQ_info.dev_type&&
		   !strcmp(cur_conf->SDAQ_info.dev_type, new_conf->SDAQ_info.dev_type) &&
		   cur_conf->SDAQ_info.firm_rev == new_conf->SDAQ_info.firm_rev &&
		   cur_conf->SDAQ_info.hw_rev == new_conf->SDAQ_info.hw_rev &&
		   cur_conf->SDAQ_info.num_of_ch == new_conf->SDAQ_info.num_of_ch &&
		   cur_conf->SDAQ_info.sample_rate == new_conf->SDAQ_info.sample_rate &&
		   cur_conf->SDAQ_info.max_cal_point == new_conf->SDAQ_info.max_cal_point)
		   retval = EXIT_SUCCESS;
		else
		{
			fprintf(stderr, "Error in Correlation: ");
			if(cur_conf->SDAQ_info.serial_number != new_conf->SDAQ_info.serial_number)
				fprintf(stderr, "cur_conf->SDAQ_info.serial_number(%d) != new_conf->SDAQ_info.serial_number(%d) !!!\n",cur_conf->SDAQ_info.serial_number, new_conf->SDAQ_info.serial_number);
			if(!cur_conf->SDAQ_info.dev_type)
				fprintf(stderr, "cur_conf->SDAQ_info.dev_type is Unknown!!!\n");
			if(!new_conf->SDAQ_info.dev_type)
				fprintf(stderr, "new_conf->SDAQ_info.dev_type is Unknown!!!\n");
			if(strcmp(cur_conf->SDAQ_info.dev_type, new_conf->SDAQ_info.dev_type))
				fprintf(stderr, "cur_conf->SDAQ_info.dev_type(%s) != new_conf->SDAQ_info.dev_type(%s) !!!\n",cur_conf->SDAQ_info.dev_type, new_conf->SDAQ_info.dev_type);

			if(cur_conf->SDAQ_info.firm_rev != new_conf->SDAQ_info.firm_rev)
				fprintf(stderr, "cur_conf->SDAQ_info.firm_rev(%d) != new_conf->SDAQ_info.firm_rev(%d) !!!\n",cur_conf->SDAQ_info.firm_rev, new_conf->SDAQ_info.firm_rev);
			if(cur_conf->SDAQ_info.hw_rev != new_conf->SDAQ_info.hw_rev)
				fprintf(stderr, "cur_conf->SDAQ_info.hw_rev(%d) != new_conf->SDAQ_info.hw_rev(%d) !!!\n",cur_conf->SDAQ_info.hw_rev, new_conf->SDAQ_info.hw_rev);
			if(cur_conf->SDAQ_info.num_of_ch != new_conf->SDAQ_info.num_of_ch)
				fprintf(stderr, "cur_conf->SDAQ_info.num_of_ch(%d) != new_conf->SDAQ_info.num_of_ch(%d) !!!\n",cur_conf->SDAQ_info.num_of_ch, new_conf->SDAQ_info.num_of_ch);
			if(cur_conf->SDAQ_info.sample_rate != new_conf->SDAQ_info.sample_rate)
				fprintf(stderr, "cur_conf->SDAQ_info.sample_rate(%d) != new_conf->SDAQ_info.sample_rate(%d) !!!\n",cur_conf->SDAQ_info.sample_rate, new_conf->SDAQ_info.sample_rate);
			if(cur_conf->SDAQ_info.max_cal_point != new_conf->SDAQ_info.max_cal_point)
				fprintf(stderr, "cur_conf->SDAQ_info.max_cal_point(%d) != new_conf->SDAQ_info.max_cal_point(%d) !!!\n",cur_conf->SDAQ_info.max_cal_point, new_conf->SDAQ_info.max_cal_point);
			retval = EXIT_FAILURE;
		}
	}
	if(options & DATE)
	{
		//printf("dates check\n");
	}
	if(options & POINTS)
	{
		//printf("points check\n");
	}
	return retval;
}

//function that send the data from SDAQ_info_cal_data to SDAQ with address: dev_addr. Return: 0 on success or 1 on failure
int set_SDAQ_info_and_calibration_data(int socket_num, unsigned char dev_addr, SDAQ_info_cal_data *new_SDAQ_cal_config)
{
	GSList *new_date_node, *new_cal_data_nodes;
	date_list_data_of_node *date_node_data;
	sdaq_calibration_points_data *cal_point_node_data;
	struct tm date={0};

	if(!new_SDAQ_cal_config || !new_SDAQ_cal_config->Calibration_date_list)
		return EXIT_FAILURE;
	for(new_date_node = (GSList *)new_SDAQ_cal_config->Calibration_date_list; new_date_node; new_date_node=new_date_node->next)
	{
		date_node_data = (date_list_data_of_node *)(new_date_node->data);
		//Load calibration date to struct tm date
		date.tm_year = 100 + date_node_data->year;
		date.tm_mon = date_node_data->month - 1;
		date.tm_mday = date_node_data->day;
		//Write CalibrationDate data to SDAQ
		if(WriteCalibrationDate(socket_num, dev_addr, date_node_data->ch_num, &date, date_node_data->period, date_node_data->amount_of_points, date_node_data->cal_unit))
			return EXIT_FAILURE;
		if(date_node_data->amount_of_points)
		{
			new_cal_data_nodes = (GSList *)new_SDAQ_cal_config->Cal_points_data_lists[date_node_data->ch_num-1];
			while(new_cal_data_nodes)
			{
				cal_point_node_data = (sdaq_calibration_points_data *)(new_cal_data_nodes->data);
				if(WriteCalibrationPoint(socket_num, dev_addr, date_node_data->ch_num, cal_point_node_data->data_of_point, cal_point_node_data->points_num, cal_point_node_data->type))
					return EXIT_FAILURE;
				new_cal_data_nodes = new_cal_data_nodes->next;
			}
		}
	}
	return EXIT_SUCCESS;
}

void free_SDAQ_info_cal_data(SDAQ_info_cal_data *conf)
{
	//Free the list and the arrays of the conf
	g_slist_free_full((GSList *)(conf->Calibration_date_list), free_SDAQ_Date_node);
	conf->Calibration_date_list = NULL;
	for(int i=0; i<conf->SDAQ_info.num_of_ch; i++)
	{
		g_slist_free_full((GSList *)(conf->Cal_points_data_lists[i]), free_SDAQ_Date_node);
		conf->Cal_points_data_lists[i] = NULL;
	}
	free(conf->Cal_points_data_lists);
	conf->Cal_points_data_lists=NULL;
}
