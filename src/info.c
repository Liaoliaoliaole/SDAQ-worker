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
typedef struct SDAQ_information_and_calibration_data
{
	struct SDAQ_info{
		unsigned int serial_number;
		const char *dev_type;	
		unsigned char firm_rev;
		unsigned char hw_rev;
		unsigned char num_of_ch;
		unsigned char sample_rate;
	}SDAQ_info;
	struct GSList *Calibration_date_list;
	struct GSlist *Calibration_point_data_lists;
}SDAQ_info_cal_data;
//struct used as container type for the data of the Calibration_date_list 
typedef struct calibration_date
{
	unsigned char ch_num;
	unsigned int date;
	unsigned char amount_of_points;
}date_list_data_of_node;

//message reception flags union. Contains a struct with the flags and the amount of available channel, 
union RX_info_calibration_date_flags_short
{
	struct{
		unsigned char amount_of_waiting_channel;
		unsigned id_status_msg_flag : 1;
		unsigned info_msg_flag : 1;
	}__attribute__((packed, aligned(1))) as_flags;
	unsigned short as_bytes;
}; 

//Global Variables
unsigned char info_TMR_exp=1;

//local functions
int get_SDAQ_info_and_calibration_data(int socket_num, unsigned char dev_addr, unsigned int scanning_time, SDAQ_info_cal_data *str);
date_list_data_of_node* new_SDAQ_date_node();//allocate memory for a new sdaq_calibration_date 
void free_SDAQ_Date_node(gpointer node);//used with g_slist_free_full to free the data of node
void printf_SDAQ_Date_node(gpointer Date_node, gpointer arg_pass);

int info(int socket_num, unsigned char dev_addr, opt_flags *usr_flag)
{
	//Local variables, SDAQ information and calibration date and data.
	SDAQ_info_cal_data str={0}; 
	int retval;
	retval = get_SDAQ_info_and_calibration_data(socket_num, dev_addr, usr_flag->timeout, &str);
	if(!retval)
	{
		printf("----- Info of SDAQ with Address %d -----\n"
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
		printf("----- Calibration Expiration Dates -----\n");						 
		g_slist_foreach((GSList *)(str.Calibration_date_list),printf_SDAQ_Date_node,NULL);
	}
	g_slist_free_full((GSList *)(str.Calibration_date_list), free_SDAQ_Date_node);
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
	//date_list_data_of_node new node work pointer; 
	date_list_data_of_node *new_node;
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
						new_node = new_SDAQ_date_node();
						//Load data from decoded "frame_rx" buffer to node
						new_node->ch_num = id_dec->channel_num;
						new_node->date = date_dec->date;
						new_node->amount_of_points = date_dec->amount_of_points; 
						str->Calibration_date_list = (struct GSList *)g_slist_append((GSList *)str->Calibration_date_list, new_node);
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
	return EXIT_SUCCESS;
	/*
	//initialize timer expired time 
	info_TMR_exp = 1;
	memset (&timer, 0, sizeof(timer));
	timer.it_value.tv_sec = scanning_time;
	timer.it_value.tv_usec = 0;
	setitimer (ITIMER_REAL, &timer, NULL);
	
	QueryCalibrationData(socket_num, dev_addr);
	while(info_TMR_exp)
	{	
		RX_bytes=read(socket_num, &frame_rx, sizeof(frame_rx));
		if(RX_bytes==sizeof(frame_rx))
		{
			if(id_dec->device_addr==dev_addr)
			{
				switch(id_dec->payload_type)
				{
					case Device_status:
						str->SDAQ_info.serial_number = status_dec->dev_sn;
						str->SDAQ_info.dev_type = dev_type_str[status_dec->dev_type];
						break;
					case Device_info:
						str->SDAQ_info.num_of_ch = info_dec->num_of_ch;
						str->SDAQ_info.sample_rate = info_dec->sample_rate;
						str->SDAQ_info.hw_rev = info_dec->hw_rev;
						str->SDAQ_info.firm_rev = info_dec->firm_rev;
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
	return EXIT_SUCCESS;
	*/
}

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

// print function for SDAQentry node
void printf_SDAQ_Date_node(gpointer Date_node, gpointer arg_pass) 
{
	char buff[60];
	struct tm * ptm;
	date_list_data_of_node *node_dec = (date_list_data_of_node *)Date_node;
	time_t exp_cal_date = node_dec->date;
	ptm = gmtime(&exp_cal_date);
	strftime (buff,sizeof(buff),"%Y/%m",ptm);
	printf("  CH%-2d : Date-> %s | Points = %d\n",node_dec->ch_num,
												  buff,
												  node_dec->amount_of_points);
	return;
}

