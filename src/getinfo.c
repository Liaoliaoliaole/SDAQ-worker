/*
File: getinfo.c, Implementation of functions for mode "getinfo"
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
#define RETRY_CNT_INIT 10 //Amount of retries for failed Calibration Point Data

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <sys/time.h>
#include <signal.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include "info.h"//including -> "SDAQ_drv.h", "Modes.h"
#include "SDAQ_xml.h"

//message reception flags union. Contains a struct with the flags and the amount of available channel,
union RX_info_calibration_date_flags_short{
	struct{
		unsigned char amount_of_waiting_channel;
		unsigned id_status_msg_flag : 1;
		unsigned info_msg_flag : 1;
	}__attribute__((packed, aligned(1))) as_flags;
	unsigned short as_bytes;
};

//Global Variables
volatile unsigned char TMR_exp=1;

	/*------ Implementation of functions------*/
int getinfo(int socket_num, unsigned char dev_addr, opt_flags *usr_flag)
{
	//Local variables, SDAQ information and calibration date and data.
	SDAQ_info_cal_data str={0};
	int retval;

	if(!(retval = get_SDAQ_info_and_calibration_data(socket_num, dev_addr, usr_flag->timeout, &str)))
	{
		if(!usr_flag->silent)
		{
			printf("\t------ Info of SDAQ with Address %d ------\n\n"
				   "\t\tHardware rev: %d\n"
				   "\t\tSoftware rev: %d\n"
				   "\t\tS/N: %d\n"
				   "\t\tType: %s\n"
				   "\t\tChannels: %d\n"
				   "\t\tSamplerate: %d\n",dev_addr,
									 	str.SDAQ_info.hw_rev,
									 	str.SDAQ_info.firm_rev,
									 	str.SDAQ_info.serial_number,
									 	str.SDAQ_info.dev_type,
										str.SDAQ_info.num_of_ch,
									 	str.SDAQ_info.sample_rate);
			if(g_slist_find_custom((GSList *)(str.Calibration_date_list), NULL, SDAQ_date_node_with_nonzero_amount_of_points_find))
			{
				printf("\n\t----- Expiration Date & Point's Data -----\n");
				g_slist_foreach((GSList *)(str.Calibration_date_list),printf_SDAQ_Date_with_points_node,str.Cal_points_data_lists);
			}
			else
				printf("\tAll channels have 0 amount of points\n");
			if(usr_flag->info_file)
				XML_info_file_write(usr_flag->info_file, &str, usr_flag->formatted_output);
			printf("\nPrint completed\n");
		}
		else
			XML_info_file_write("-", &str, usr_flag->formatted_output);
	}
	//free the list and the arrays
	g_slist_free_full((GSList *)(str.Calibration_date_list), free_SDAQ_Date_node);
	str.Calibration_date_list=NULL;
	if(str.Cal_points_data_lists)
	{
		for(int i=0; i<str.SDAQ_info.num_of_ch; i++)
		{
			g_slist_free_full((GSList *)(str.Cal_points_data_lists[i]), free_SDAQ_Date_node);
			str.Cal_points_data_lists[i]=NULL;
		}
		free(str.Cal_points_data_lists);
	}
	return retval;
}

void info_timer_handler (int signum)
{
	TMR_exp = 0;
	return;
}

int get_SDAQ_info_and_calibration_data(int socket_num, unsigned char dev_addr, unsigned int scanning_time, SDAQ_info_cal_data *str)
{
	int ret_val;
	if(!(ret_val = get_SDAQ_info(socket_num, dev_addr, scanning_time, str)))
		ret_val = get_SDAQ_calibration_data(socket_num, dev_addr, scanning_time, str, NULL);
	return ret_val;
}

int get_SDAQ_info(int socket_num, unsigned char dev_addr, unsigned int scanning_time, SDAQ_info_cal_data *str)
{
	//Union with flags and a counter with the amount of channels. Each flag zero on reception. amount_of_waiting_channel decreases in reception.
	union RX_info_calibration_date_flags_short rfb = {.as_flags.id_status_msg_flag=1, .as_flags.info_msg_flag=1};
	//CAN Socket and SDAQ related variables
	struct can_frame frame_rx;
	unsigned char Channel;
	int RX_bytes;
	GSList *list_node;
	sdaq_can_id *id_dec = (sdaq_can_id *)&(frame_rx.can_id);
	sdaq_status *status_dec = (sdaq_status *)frame_rx.data;
	sdaq_info   *info_dec   = (sdaq_info *)frame_rx.data;
	sdaq_calibration_date *date_dec = (sdaq_calibration_date *)frame_rx.data;
	date_list_data_of_node *new_date_node; //date_list_data_of_node work pointer;
	//Timers related Variables
	struct itimerval timer;//Scan Timeout

	//initialize timer expired time
	TMR_exp = 1;
	memset (&timer, 0, sizeof(timer));
	timer.it_value.tv_sec = scanning_time;
	timer.it_value.tv_usec = 0;
	setitimer (ITIMER_REAL, &timer, NULL);

	//link signal SIGALRM to timer's handler
	signal(SIGALRM, info_timer_handler);

	//Request SDAQ's info. Wait to received Status/SN, Dev_Info, and calibration date for each channel
	QueryDeviceInfo(socket_num, dev_addr);
	while(TMR_exp && rfb.as_bytes)
	{
		RX_bytes=read(socket_num, &frame_rx, sizeof(frame_rx));
		if(RX_bytes==sizeof(frame_rx))
		{
			if(id_dec->device_addr==dev_addr)
			{
				switch(id_dec->payload_type)
				{
					case Device_status:
						if(rfb.as_flags.id_status_msg_flag)
						{
							str->SDAQ_info.serial_number = status_dec->dev_sn;
							str->SDAQ_info.dev_type = dev_type_str[status_dec->dev_type];
							rfb.as_flags.id_status_msg_flag = 0;
						}
						break;
					case Device_info:
						if(rfb.as_flags.info_msg_flag)
						{
							str->SDAQ_info.num_of_ch = info_dec->num_of_ch;
							str->SDAQ_info.sample_rate = info_dec->sample_rate;
							str->SDAQ_info.hw_rev = info_dec->hw_rev;
							str->SDAQ_info.firm_rev = info_dec->firm_rev;
							str->SDAQ_info.max_cal_point = info_dec->max_cal_point;
							rfb.as_flags.info_msg_flag = 0;
							rfb.as_flags.amount_of_waiting_channel = info_dec->num_of_ch;
						}
						break;
					case Calibration_Date:
						if(rfb.as_flags.amount_of_waiting_channel)
						{
							Channel = id_dec->channel_num;
							if(!(list_node = g_slist_find_custom((GSList *)(str->Calibration_date_list), &Channel, SDAQ_date_node_with_channel_b_find)))
								new_date_node = new_SDAQ_date_node();
							else
								new_date_node = (date_list_data_of_node *)list_node->data;
							//Load data from decoded "frame_rx" buffer to node
							new_date_node->ch_num = Channel;
							new_date_node->year = date_dec->year;
							new_date_node->month = date_dec->month;
							new_date_node->day = date_dec->day;
							new_date_node->period = date_dec->period;
							new_date_node->amount_of_points = date_dec->amount_of_points;
							new_date_node->cal_unit = date_dec->cal_units;
							if(!list_node)
								str->Calibration_date_list = (struct GSList *)g_slist_append((GSList *)str->Calibration_date_list, new_date_node);
							rfb.as_flags.amount_of_waiting_channel--;
						}
						break;
				}
			}
		}
		else
		{
			printf("No device found\n");
			return EXIT_FAILURE;
		}
	}
	if(rfb.as_bytes)
	{
		printf("Reception Failed\n");
		return EXIT_FAILURE;
	}
	if(!str->Cal_points_data_lists)
	{
		str->Cal_points_data_lists = calloc(str->SDAQ_info.num_of_ch, sizeof(struct GSList *));
		if(!str->Cal_points_data_lists)
		{
			fprintf(stderr,"Memory Error\n");
			exit(EXIT_FAILURE);
		}
	}
	return TMR_exp ? EXIT_SUCCESS : EXIT_FAILURE;
}

int get_SDAQ_calibration_data(int socket_num, unsigned char dev_addr, unsigned int scanning_time, SDAQ_info_cal_data *str, void **CH_Req)
{
	//CAN Socket and SDAQ related variables
	struct can_frame frame_rx;
	int RX_bytes, retry_cnt = RETRY_CNT_INIT;
	GSList *list_node;
	sdaq_can_id *id_dec = (sdaq_can_id *)&(frame_rx.can_id);
	sdaq_calibration_date *date_dec = (sdaq_calibration_date *)frame_rx.data;
	date_list_data_of_node *new_date_node; //date_list_data_of_node work pointer;
	sdaq_calibration_points_data *point_node; //sdaq_calibration_points_data work pointer;
	//Timers related Variables
	struct itimerval timer;//Scan Timeout

	if(str->SDAQ_info.num_of_ch<=0)
		return EXIT_FAILURE;

	if(!str->Cal_points_data_lists)
	{
		str->Cal_points_data_lists = calloc(str->SDAQ_info.num_of_ch, sizeof(struct GSList *));
		if(!str->Cal_points_data_lists)
		{
			fprintf(stderr,"Memory Error\n");
			exit(EXIT_FAILURE);
		}
	}

	//Request SDAQ's info. Wait to received Calibration data points. Recall for each channel
	for(int i=0,cnt; i<str->SDAQ_info.num_of_ch; i++)
	{
		TMR_exp = 1;
		if(CH_Req)
		{
			list_node = ((GSList **)CH_Req)[i];
			if(!list_node)
				continue;
		}
		//initialize timer expired time to 250 msec
		memset (&timer, 0, sizeof(timer));
		timer.it_value.tv_sec = 0;
		timer.it_value.tv_usec = 250000;
		setitimer (ITIMER_REAL, &timer, NULL);
		cnt=0;
		QueryCalibrationData(socket_num, dev_addr, i+1);
		while(TMR_exp && cnt < str->SDAQ_info.max_cal_point*6+1)//6 is the amount of data in a point (meas, ref, offset, gain, C2, C3) + 1 for the extra Calibration_Date message
		{
			RX_bytes=read(socket_num, &frame_rx, sizeof(frame_rx));
			if(RX_bytes==sizeof(frame_rx))
			{
				if(id_dec->device_addr == dev_addr)
				{
					switch(id_dec->payload_type)
					{
						case Calibration_Point_Data:
							//Check if Point is already in the Cal_points_data_lists[i].
							if(!(list_node = g_slist_find_custom((GSList *)(str->Cal_points_data_lists[i]), frame_rx.data, SDAQ_point_node_with_type_and_num_find)))
								point_node = new_SDAQ_cal_point_node();
							else
								point_node = (sdaq_calibration_points_data *)list_node->data;
							memcpy(point_node, frame_rx.data, sizeof(sdaq_calibration_points_data));
							if(!list_node)
								str->Cal_points_data_lists[i] = (struct GSList *)g_slist_append((GSList *)(str->Cal_points_data_lists[i]), point_node);
							cnt++;
							break;
						case Calibration_Date:
							if((new_date_node = g_slist_nth_data((GSList *)str->Calibration_date_list, i)))
							{
								if(new_date_node->ch_num == id_dec->channel_num)
								{
									//Check diffs and Reload data from decoded "frame_rx" buffer to node
									if(new_date_node->amount_of_points != date_dec->amount_of_points)
										new_date_node->amount_of_points = date_dec->amount_of_points;
									if(new_date_node->cal_unit != date_dec->cal_units)
										new_date_node->cal_unit = date_dec->cal_units;
									if(new_date_node->period != date_dec->period)
										new_date_node->period = date_dec->period;
									if(new_date_node->year != date_dec->year || new_date_node->month != date_dec->month || new_date_node->day != date_dec->day)
									{
										new_date_node->year = date_dec->year;
										new_date_node->month = date_dec->month;
										new_date_node->day = date_dec->day;
									}
								}
								else
								{
									printf("Fatal Error@Rx of CalibrationData: Data for CH_%02d received in wrong order!!!\n", i+1);
									return EXIT_FAILURE;
								}
							}
							else
							{
								new_date_node = new_SDAQ_date_node();
								//Load data from decoded "frame_rx" buffer to node
								new_date_node->ch_num = id_dec->channel_num;
								new_date_node->year = date_dec->year;
								new_date_node->month = date_dec->month;
								new_date_node->day = date_dec->day;
								new_date_node->period = date_dec->period;
								new_date_node->amount_of_points = date_dec->amount_of_points;
								new_date_node->cal_unit = date_dec->cal_units;
								str->Calibration_date_list = (struct GSList *)g_slist_append((GSList *)str->Calibration_date_list, new_date_node);
							}
							cnt++;
							break;
					}
				}
			}
			else
			{
				if(str->Cal_points_data_lists[i])
				{
					g_slist_free_full((GSList *)(str->Cal_points_data_lists[i]), free_SDAQ_Date_node);
					str->Cal_points_data_lists[i] = NULL;
				}
				i--;
				if(!retry_cnt--)
				{
					printf("Get of CalibrationData Failed, too many reties (>%d)!!!\n", RETRY_CNT_INIT);
					return EXIT_FAILURE;
				}
			}
		}
	}
	return TMR_exp ? EXIT_SUCCESS : EXIT_FAILURE;
}

gint SDAQ_point_node_with_type_and_num_find(gconstpointer a, gconstpointer b)//GFunc function used with g_slist_find_custom.
{
	const sdaq_calibration_points_data *current_point_node = (sdaq_calibration_points_data* )a,
									   *wanted_point_num = (sdaq_calibration_points_data* )b;

	return current_point_node->type == wanted_point_num->type && current_point_node->points_num == wanted_point_num->points_num ? 0 : 1;
}

/*---- Implementation of function for Calibration_date_list ----*/
// Allocates space for a new SDAQ entrance
date_list_data_of_node* new_SDAQ_date_node()
{
    date_list_data_of_node *new_date_node_data = g_slice_alloc0(sizeof(date_list_data_of_node));
    if(!new_date_node_data)
	{
		fprintf(stderr,"Memory Error\n");
		exit(EXIT_FAILURE);
	}
	return new_date_node_data;
}
// frees the allocated space for struct SDAQentry and its data
void free_SDAQ_Date_node(gpointer Date_node)
{
    if(Date_node)
	{
		g_slice_free(date_list_data_of_node, Date_node);
		Date_node = NULL;
	}
}

/*---- Declaration of function for Cal_points_data_lists ----*/
//allocate memory for a new sdaq_calibration_points_data part of Cal_points_data_lists
sdaq_calibration_points_data* new_SDAQ_cal_point_node()
{
	sdaq_calibration_points_data *new_point_node_data = (sdaq_calibration_points_data *) g_slice_alloc0(sizeof(sdaq_calibration_points_data));
    if(!new_point_node_data)
	{
		fprintf(stderr,"Memory Error\n");
		exit(EXIT_FAILURE);
	}
	return new_point_node_data;
}
//used with g_slist_free_full to free the data of node
void free_SDAQ_cal_point_node(gpointer point_node)
{
	if(point_node)
	{
		g_slice_free(sdaq_calibration_points_data, point_node);
		point_node = NULL;
	}
}

/*
	Comparing function used in g_slist_find_custom.
	Find the first node with non zero value on amount_of_points field. Argument b does not used.
*/
gint SDAQ_date_node_with_nonzero_amount_of_points_find (gconstpointer a, gconstpointer b)
{
	return ((date_list_data_of_node *)a)->amount_of_points > 0 ?  0 : 1;
}

//assist function prints the Data of the points. arg_pass is a pointer to an integer with the amount_of_points that will be print out.
void printf_SDAQ_cal_point_node(gpointer Point_node, gpointer arg_pass)
{
	unsigned char amount_of_points = *((unsigned char *) arg_pass);
	sdaq_calibration_points_data *node_dec = (sdaq_calibration_points_data*) Point_node;
	if(node_dec->points_num<amount_of_points)
	{
		switch(node_dec->type)
		{
			case meas:
				printf(" | %2d | %9.3g  | ",node_dec->points_num+1, node_dec->data_of_point);
				break;
			case ref:
				printf(" %9.3g | ",node_dec->data_of_point);
				break;
			case offset:
				printf(" %9.3g | ",node_dec->data_of_point);
				break;
			case gain:
				printf(" %9.3g | ",node_dec->data_of_point);
				break;
			case C2:
				printf(" %9.3g | ",node_dec->data_of_point);
				break;
			case C3:
				printf(" %9.3g |\n",node_dec->data_of_point);
				if(node_dec->points_num<amount_of_points-1)
					printf(" |----|------------|------------|------------|------------|------------|------------|\n");
				break;
			//default :
		}
	}
	return;
}


//Called from g_slist_foreach. the pass_arg is the array with with the list of calibration data points
void printf_SDAQ_Date_with_points_node(gpointer Date_node, gpointer arg_pass)
{
	GSList **point_data_lists = (GSList **) arg_pass;
	char buff[60];
	struct tm ptm = {0};
	date_list_data_of_node *node_dec = Date_node;

	ptm.tm_year = node_dec->year + 100; //100 = 2000-1900
	ptm.tm_mon = node_dec->month - 1;
	ptm.tm_mday =  node_dec->day;
	strftime(buff,sizeof(buff),"%Y/%m/%d",&ptm);
	if(node_dec->amount_of_points)
	{
		printf("   CH%02d: Calibrated @ %s valid for %3d Months, Cal_Points = %2d, Unit = %s%s\n", node_dec->ch_num,
																								   buff,
																								   node_dec->period,
																							  	   node_dec->amount_of_points,
																							  	   unit_str[node_dec->cal_unit],
																							  	   node_dec->cal_unit<Unit_code_base_region_size?"(Base)":"");
		printf(" /----------------------------------------------------------------------------------\\\n"
			   " | #  |   Measure  |  Reference |    Offset  |    Gain    |      C2    |      C3    |\n"
		       " |----|------------|------------|------------|------------|------------|------------|\n");
		g_slist_foreach((GSList *)(point_data_lists[node_dec->ch_num-1]),printf_SDAQ_cal_point_node,&(node_dec->amount_of_points));
		printf(" \\----------------------------------------------------------------------------------/\n");
	}

	return;
}
