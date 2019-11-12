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

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "SDAQ_drv.h"
#include "Modes.h"

/*	Struct SDAQ_information_and_calibration_data
		used in mode info and calibration. 
		Contains:
			 internal struct SDAQ info. 
			 A List with calibration points data (aka sdaq_calibration_data) for each channel
			 A list with dates and amount of data (aka sdaq_calibration_date) for each channel
*/
typedef struct SDAQ_information_and_calibration_data{
	struct SDAQ_info{
		unsigned int serial_number;
		const char *dev_type;	
		unsigned char firm_rev;
		unsigned char hw_rev;
		unsigned char num_of_ch;
		unsigned char sample_rate;
	}SDAQ_info;
	struct GSList *Calibration_date_list;
	struct GSlist **Calibration_point_data_lists;//array of lists 
}SDAQ_info_cal_data;
//struct used as container type for the data of the Calibration_date_list 
typedef struct calibration_date{
	unsigned char ch_num;
	unsigned int date;
	unsigned char amount_of_points;
}date_list_data_of_node;

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
volatile unsigned char info_TMR_exp=1;

//local functions
int get_SDAQ_info_and_calibration_data(int socket_num, unsigned char dev_addr, unsigned int scanning_time, SDAQ_info_cal_data *str);
//Declaration of function for Calibration_date_list 
date_list_data_of_node* new_SDAQ_date_node();//allocate memory for a new sdaq_calibration_date 
void free_SDAQ_Date_node(gpointer Date_node);//used with g_slist_free_full to free the data of node
//Declaration of function for Calibration_point_data_lists 
sdaq_calibration_points_data* new_SDAQ_cal_point_node();//allocate memory for a new sdaq_calibration_points_data part of Calibration_point_data_lists 
void free_SDAQ_cal_point_node(gpointer Point_node);//used with g_slist_free_full to free the data of node
//Called from g_slist_foreach. the pass_arg is the array with with the list of calibration data points
void printf_SDAQ_Date_with_points_node(gpointer Date_node, gpointer pass_arg);

int info(int socket_num, unsigned char dev_addr, opt_flags *usr_flag)
{
	//Local variables, SDAQ information and calibration date and data.
	SDAQ_info_cal_data str={0}; 
	int retval;
	retval = get_SDAQ_info_and_calibration_data(socket_num, dev_addr, usr_flag->timeout, &str);
	if(!retval)
	{
		printf("------ Info of SDAQ with Address %d ------\n"
			   "\tHardware rev: %d\n"
			   "\tSoftware rev: %d\n"
			   "\tS/N: %d\n"
			   "\tType: %s\n"
			   "\tChannels: %d\n"
			   "\tSamplerate: %d\n",dev_addr,
								 str.SDAQ_info.hw_rev,
								 str.SDAQ_info.firm_rev,
								 str.SDAQ_info.serial_number,
								 str.SDAQ_info.dev_type,
								 str.SDAQ_info.num_of_ch,
								 str.SDAQ_info.sample_rate);
		printf("----- Expiration Date & Point's Data -----\n");						 
		g_slist_foreach((GSList *)(str.Calibration_date_list),printf_SDAQ_Date_with_points_node,str.Calibration_point_data_lists);
	}
	//free the list and the arrays
	g_slist_free_full((GSList *)(str.Calibration_date_list), free_SDAQ_Date_node);
	for(int i=0; i<str.SDAQ_info.num_of_ch; i++)
		g_slist_free_full((GSList *)(str.Calibration_point_data_lists[i]), free_SDAQ_Date_node);
	free(str.Calibration_point_data_lists);
	return retval;
}

void info_timer_handler (int signum)
{
	info_TMR_exp = 0;
	return;
}

int get_SDAQ_info_and_calibration_data(int socket_num, unsigned char dev_addr, unsigned int scanning_time, SDAQ_info_cal_data *str)
{
	//Union with flags and a counter with the amount of channels. Each flag zero on reception. amount_of_waiting_channel decreases in reception.  
	union RX_info_calibration_date_flags_short rfb = {.as_flags.id_status_msg_flag=1,.as_flags.info_msg_flag=1};  
	//CAN Socket and SDAQ related variables
	struct can_frame frame_rx;
	int RX_bytes;
	sdaq_can_id *id_dec = (sdaq_can_id *)&(frame_rx.can_id); 
	sdaq_status *status_dec = (sdaq_status *)frame_rx.data;
	sdaq_info   *info_dec   = (sdaq_info *)frame_rx.data;
	sdaq_calibration_date *date_dec = (sdaq_calibration_date *)frame_rx.data;
	date_list_data_of_node *new_date_node; //date_list_data_of_node work pointer;
	sdaq_calibration_points_data *new_point_node; //sdaq_calibration_points_data work pointer;	
	//Timers related Variables
	struct itimerval timer;//Scan Timeout
	
	//link signal SIGALRM to timer's handler
	signal(SIGALRM, info_timer_handler);

	//initialize timer expired time 
	info_TMR_exp = 1;
	memset (&timer, 0, sizeof(timer));
	timer.it_value.tv_sec = scanning_time;
	timer.it_value.tv_usec = 0;
	setitimer (ITIMER_REAL, &timer, NULL);
	
	//Request SDAQ's info. Wait to received Status/SN, Dev_Info, and calibration date for each channel
	QueryDeviceInfo(socket_num, dev_addr);
	while(info_TMR_exp && rfb.as_bytes)
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
							rfb.as_flags.info_msg_flag = 0;
							rfb.as_flags.amount_of_waiting_channel = info_dec->num_of_ch; 
						}
						break;
					case Calibration_Date:
						new_date_node = new_SDAQ_date_node();
						//Load data from decoded "frame_rx" buffer to node
						new_date_node->ch_num = id_dec->channel_num;
						new_date_node->date = date_dec->date;
						new_date_node->amount_of_points = date_dec->amount_of_points; 
						str->Calibration_date_list = (struct GSList *)g_slist_append((GSList *)str->Calibration_date_list, new_date_node);
						rfb.as_flags.amount_of_waiting_channel--;
						break;
				}
			}
		}
		else
		{
			printf("Timeout\n");
			return EXIT_FAILURE;
		}
	}
	if(rfb.as_bytes)
	{
		printf("Reception Failed\n");
		return EXIT_FAILURE;
	}
	str->Calibration_point_data_lists = calloc(str->SDAQ_info.num_of_ch, sizeof(struct GSlist *));
	if(!str->Calibration_point_data_lists)
	{
		fprintf(stderr,"Memory Error\n");
		exit(EXIT_FAILURE);
	}
	//Request SDAQ's info. Wait to received Calibration data points. Recall for each channel
	for(int i=0,cnt; i<str->SDAQ_info.num_of_ch; i++)
	{
		//initialize timer expired time 
		info_TMR_exp = 1;
		memset (&timer, 0, sizeof(timer));
		timer.it_value.tv_sec = scanning_time;
		timer.it_value.tv_usec = 0;
		setitimer (ITIMER_REAL, &timer, NULL);
		cnt=0;//for 8 input + 8 output and + 1
		QueryCalibrationData(socket_num, dev_addr, i+1);
		while(info_TMR_exp && cnt<16)
		{	
			RX_bytes=read(socket_num, &frame_rx, sizeof(frame_rx));
			if(RX_bytes==sizeof(frame_rx))
			{
				if(id_dec->device_addr == dev_addr && id_dec->payload_type == Calibration_Point_Data)
				{
					new_point_node = new_SDAQ_cal_point_node();					
					memcpy(new_point_node, frame_rx.data, sizeof(sdaq_calibration_points_data));
					(str->Calibration_point_data_lists)[i] = (struct GSlist *)g_slist_append((GSList *)(str->Calibration_point_data_lists)[i], new_point_node);
					cnt++;//inc cnt of the success receptions  
				}
			}
			else
			{
				printf("Timeout\n");
				return EXIT_FAILURE;
			}
		}
	}
	return EXIT_SUCCESS;
}

/*---- Implementation of function for Calibration_date_list ----*/ 
// Allocates space for a new SDAQ entrance
date_list_data_of_node* new_SDAQ_date_node()
{
    date_list_data_of_node *new_date_node_data = (date_list_data_of_node *) g_slice_alloc(sizeof(date_list_data_of_node));
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
    g_slice_free(date_list_data_of_node, Date_node);
} 

/*---- Declaration of function for Calibration_point_data_lists ----*/
//allocate memory for a new sdaq_calibration_points_data part of Calibration_point_data_lists 
sdaq_calibration_points_data* new_SDAQ_cal_point_node()
{
	sdaq_calibration_points_data *new_point_node_data = (sdaq_calibration_points_data *) g_slice_alloc(sizeof(sdaq_calibration_points_data));
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
	g_slice_free(sdaq_calibration_points_data, point_node);
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
			case Input  : 
				printf("    Point %d-> %#4.3f Input ",node_dec->points_num, node_dec->data_of_point);
				break;
			case Output : 
				printf("| %4.3f Output\n",node_dec->data_of_point); 
				break;
			//default : 
		}
		
	}
	return;
}


//Called from g_slist_foreach. the pass_arg is the array with with the list of calibration data points
void printf_SDAQ_Date_with_points_node(gpointer Date_node, gpointer arg_pass) 
{
	struct GSlist **point_data_lists = (struct GSlist **) arg_pass;
	char buff[60];
	struct tm * ptm;
	date_list_data_of_node *node_dec = (date_list_data_of_node *)Date_node;
	time_t exp_cal_date = node_dec->date;
	ptm = gmtime(&exp_cal_date);
	strftime (buff,sizeof(buff),"%Y/%m",ptm);
	printf("  CH%02d: Expired @ %s | Points = %d\n",node_dec->ch_num,
												  buff,
												  node_dec->amount_of_points);
	g_slist_foreach((GSList *)(point_data_lists[node_dec->ch_num-1]),printf_SDAQ_cal_point_node,&(node_dec->amount_of_points));
	return;
}


