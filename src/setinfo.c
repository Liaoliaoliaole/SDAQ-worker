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

	//--- Local Functions declaration ---//
//Function for decode external command
int str_dec(char **arg, char *input_buff, const char *delim);
//Function for construction of struct tm with calibration date of SDAQ
int date_to_tm(struct tm *output_date, char *input_buff);
//Function that return the amount of channels with points
int cnt_conf_CHs(SDAQ_info_cal_data * conf);

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
		if(!usr_flag->silent)
		{
			printf("XML file read and validation: ");
			fflush(stdout);
		}
		if(XML_info_file_read_and_validate(usr_flag->info_file, &new_conf))
		{
			free_SDAQ_info_cal_data(&new_conf);
			return EXIT_FAILURE;
		}
		if(!usr_flag->silent)
		{
			printf(" Success\nGet Calibration info from SDAQ: ");
			fflush(stdout);
		}
	}
	if(!(retval = get_SDAQ_info(socket_num, dev_addr, usr_flag->timeout, &cur_conf)))
	{
		if(usr_flag->info_file)
		{
			if(!usr_flag->silent)
			{
				printf("Success\nCorrelation SDAQ<>new_config: ");
				fflush(stdout);
			}
			if(!corr_SDAQ_info_and_calibration_data(&cur_conf, &new_conf, INFO))
			{
				if(!usr_flag->silent)
				{
					printf("Success\nSend new_config to SDAQ: ");
					fflush(stdout);
				}
				if(!(retval = set_SDAQ_info_and_calibration_data(socket_num, dev_addr, &new_conf)))
				{
					if(!usr_flag->silent)
						printf("Success\n");
					if(usr_flag->verify)
					{
						if(!usr_flag->silent)
						{
							printf("Verification: ");
							fflush(stdout);
						}
						if(!cnt_conf_CHs(&new_conf))
							retval = get_SDAQ_info(socket_num, dev_addr, usr_flag->timeout, &cur_conf);
						else
						{
							if(!(retval = get_SDAQ_calibration_data(socket_num, dev_addr, usr_flag->timeout, &cur_conf, (void **)new_conf.Cal_points_data_lists)))
							{
								if(!(retval = corr_SDAQ_info_and_calibration_data(&cur_conf, &new_conf, DATE|POINTS)))
								{
									if(!usr_flag->silent)
										printf("\tSuccess\n");
								}
							}
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

int cnt_conf_CHs(SDAQ_info_cal_data *conf)
{
	int i,cnt;
	if(!conf->Cal_points_data_lists)
		return 0;
	for(i=0, cnt=0; i<conf->SDAQ_info.num_of_ch; i++)
		if(conf->Cal_points_data_lists[i])
			cnt++;
	return cnt;
}

int corr_SDAQ_info_and_calibration_data(SDAQ_info_cal_data *cur_conf, SDAQ_info_cal_data *new_conf, unsigned char options)
{
	int retval = EXIT_FAILURE;
	char new_unit_str[10],cur_unit_str[10];
	GSList *cur_date_list_data_of_nodes, *new_date_list_data_of_nodes, *cur_date_list_data_node;
	date_list_data_of_node *cur_date_data, *new_date_data;
	GSList *cur_cal_point_list, *new_cal_point_list;
	sdaq_calibration_points_data *cur_point_data, *new_point_data;

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
		if(!(new_date_list_data_of_nodes = (GSList *)new_conf->Calibration_date_list)||!(cur_date_list_data_of_nodes = (GSList *)cur_conf->Calibration_date_list))
		{
			if(!new_date_list_data_of_nodes)
				fprintf(stderr, "new_conf->Calibration_date_list is undefined !!!\n");
			if(!cur_date_list_data_of_nodes)
				fprintf(stderr, "cur_conf->Calibration_date_list is undefined !!!\n");
			return EXIT_FAILURE;
		}
		while(new_date_list_data_of_nodes)
		{
			new_date_data = (date_list_data_of_node *)new_date_list_data_of_nodes->data;
			if((cur_date_list_data_node = g_slist_find_custom(cur_date_list_data_of_nodes, &(new_date_data->ch_num), SDAQ_date_node_with_channel_b_find)))
			{
				cur_date_data = (date_list_data_of_node *)cur_date_list_data_node->data;
				if(cur_date_data->year != new_date_data->year || cur_date_data->month != new_date_data->month || cur_date_data->day != new_date_data->day)
				{
					fprintf(stderr, "Calibration Date of CH%d(%d/%d/%d) is different from the configuration (%d/%d/%d)!!!\n", new_date_data->ch_num,
																															  cur_date_data->year+2000,cur_date_data->month,cur_date_data->day,
																															  new_date_data->year+2000,new_date_data->month,new_date_data->day);
					return EXIT_FAILURE;
				}
				if(cur_date_data->period != new_date_data->period)
				{
					fprintf(stderr, "Calibration period of CH%d(%d) is different from the configuration (%d)!!!\n", new_date_data->ch_num, cur_date_data->period, new_date_data->period);
					return EXIT_FAILURE;
				}
				if(cur_date_data->amount_of_points != new_date_data->amount_of_points)
				{
					fprintf(stderr, "Amount of points for CH%d(%d) is different from the configuration (%d)!!!\n", new_date_data->ch_num, cur_date_data->period, new_date_data->period);
					return EXIT_FAILURE;
				}
				if(cur_date_data->cal_unit != new_date_data->cal_unit)
				{
					sprintf(cur_unit_str, "%s%s", unit_str[cur_date_data->cal_unit], cur_date_data->cal_unit<Unit_code_base_region_size?"(Base)":"");
					sprintf(new_unit_str, "%s%s", unit_str[new_date_data->cal_unit], new_date_data->cal_unit<Unit_code_base_region_size?"(Base)":"");
					fprintf(stderr, "Calibration Unit of CH%d(%s) is different from the configuration (%s)!!!\n", new_date_data->ch_num, cur_unit_str, new_unit_str);
					return EXIT_FAILURE;
				}
			}
			else
			{
				fprintf(stderr, "date node for CH%d was not found at configurations of the SDAQ!!!\n", new_date_data->ch_num);
				return EXIT_FAILURE;
			}
			new_date_list_data_of_nodes = new_date_list_data_of_nodes->next;
		}
		retval = EXIT_SUCCESS;
	}
	if(options & POINTS)
	{
		if(!new_conf->Cal_points_data_lists || !cur_conf->Cal_points_data_lists)
		{
			if(!new_conf->Cal_points_data_lists)
				fprintf(stderr, "new_conf->Cal_points_data_lists is undefined !!!\n");
			if(!cur_conf->Cal_points_data_lists)
				fprintf(stderr, "cur_conf->Cal_points_data_lists is undefined !!!\n");
			return EXIT_FAILURE;
		}
		for(int ch=0; ch<new_conf->SDAQ_info.num_of_ch; ch++)
		{
			if(!(new_cal_point_list = (GSList *)new_conf->Cal_points_data_lists[ch]))
				continue;
			if(!(cur_cal_point_list = (GSList *)cur_conf->Cal_points_data_lists[ch]))
			{
				fprintf(stderr, "cur_conf->Cal_points_data_lists[%d] is undefined !!!\n",ch+1);
				return EXIT_FAILURE;
			}
			while(new_cal_point_list && cur_cal_point_list)
			{

				cur_point_data = (sdaq_calibration_points_data *)(cur_cal_point_list->data);
				new_point_data = (sdaq_calibration_points_data *)(new_cal_point_list->data);
				if(cur_point_data->data_of_point != new_point_data->data_of_point ||
				   cur_point_data->type != new_point_data->type ||
				   cur_point_data->points_num != new_point_data->points_num)
				{
					fprintf(stderr, "Error @ CH%d Point_%d: ",ch+1, new_point_data->points_num);
					if(cur_point_data->data_of_point != new_point_data->data_of_point)
						fprintf(stderr, "cur_point_Val=%f != new_point_val= %f", cur_point_data->data_of_point, new_point_data->data_of_point);
					if(cur_point_data->type != new_point_data->type)
						fprintf(stderr, "cur_point_type=%s != new_point_type= %s", type_of_point_str[cur_point_data->type], type_of_point_str[new_point_data->type]);
					fprintf(stderr, "\n");
					return EXIT_FAILURE;
				}
				new_cal_point_list = new_cal_point_list->next;
				cur_cal_point_list = cur_cal_point_list->next;
			}
			if(new_cal_point_list && !cur_cal_point_list)
			{
				fprintf(stderr, "sizeof(cur_conf->Cal_points_data_lists[%d]) < sizeof(new_conf->Cal_points_data_lists[%d])!!!\n",ch+1, ch+1);
				return EXIT_FAILURE;
			}
		}

		retval = EXIT_SUCCESS;
	}
	return retval;
}

//Function that send the data from SDAQ_info_cal_data to SDAQ with address: dev_addr. Return: 0 on success or 1 on failure
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
		if(date_node_data->amount_of_points)
		{
			//Write CalibrationDate data with 0 amount_of_points to enable SDAQ calibration editing.
			if(WriteCalibrationDate(socket_num, dev_addr, date_node_data->ch_num, &date, date_node_data->period, 0, date_node_data->cal_unit))
			{
				fprintf(stderr, "Failure at WriteCalibrationDate() for CH%d\n",date_node_data->ch_num);
				return EXIT_FAILURE;
			}
			new_cal_data_nodes = (GSList *)new_SDAQ_cal_config->Cal_points_data_lists[date_node_data->ch_num-1];
			while(new_cal_data_nodes)
			{
				cal_point_node_data = (sdaq_calibration_points_data *)(new_cal_data_nodes->data);
				if(WriteCalibrationPoint(socket_num, dev_addr, date_node_data->ch_num, cal_point_node_data->data_of_point, cal_point_node_data->points_num, cal_point_node_data->type))
				{
					fprintf(stderr, "Failure at WriteCalibrationPoint() for CH%d at point %d\n",date_node_data->ch_num, cal_point_node_data->points_num);
					return EXIT_FAILURE;
				}
				new_cal_data_nodes = new_cal_data_nodes->next;
			}
		}
		//Write CalibrationDate data to SDAQ
		if(WriteCalibrationDate(socket_num, dev_addr, date_node_data->ch_num, &date, date_node_data->period, date_node_data->amount_of_points, date_node_data->cal_unit))
		{
			fprintf(stderr, "Failure at WriteCalibrationDate() for CH%d\n",date_node_data->ch_num);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

/*
	Comparing function used in g_slist_find_custom.
	Compare ch_num from date_list_data_of_node a with ch_num from b. Return 0 if are the same, 1 otherwise.
*/
gint SDAQ_date_node_with_channel_b_find (gconstpointer a, gconstpointer b)
{
	const unsigned char current_ch_num = ((date_list_data_of_node*)a)->ch_num,
						wanted_ch_num = ((date_list_data_of_node*)b)->ch_num;

	return wanted_ch_num == current_ch_num ? 0 : 1;
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
