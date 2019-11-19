/*
Program: SDAQ_psim. A virtual device simulator for SDAQ-CAN Devices.
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
#define TIME_REF 100 //loop time ref
#define Stat_ID_Interval 10000/TIME_REF //for 10 sec with base time TIME_REF
#define Sync_Status_Interval 120/(Stat_ID_Interval) //for 120 seconds reset time for In_Sync flag based on Stat_ID_Interval

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <pthread.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <linux/can.h>
#include <linux/can/raw.h>

//Include SDAQ Driver header
#include "SDAQ_drv.h"

//struct definition of memory space of a pseudo_SDAQ
struct pSDAQ_memory_space
{
	unsigned char status;
	unsigned char address;
	unsigned char number_of_channels;
	float out_val[16];
	sdaq_calibration_date ch_cal_date[16];
	float data_ref_values[16][8];
	float data_mes_values[16][8];
};

//Global variables
unsigned char SDAQ_psim_run=1, active_threads=0;
pthread_mutex_t thread_make_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t *SDAQs_mem_access;

struct thread_arguments_passer
{
	char *can_if_name;
	unsigned int serial_number;
	struct pSDAQ_memory_space *pSDAQ_mem;
};

//application functions
void print_usage(char *prog_name);
short dev_ref_time_diff_cal(unsigned short dev_time, unsigned short ref_time);
void * pseudo_SDAQ(void *varg_pt);//Thread function. Act as an pseudo_SDAQ.

void handle_sigint(int sig)
{
    SDAQ_psim_run = 0;
}

int main(int argc, char *argv[])
{
	unsigned int num_of_pSDAQ;
	//char usr_com[50];
	//variables for threads
	pthread_t *CAN_socket_RX_Thread_id;
	struct thread_arguments_passer thread_arg;
	struct pSDAQ_memory_space *pSDAQs_mem;
	if(argc <= 2|| argv[1]==NULL || argv[2]==NULL)
	{
		print_usage(argv[0]);
		exit(1);
	}
	//init pseudo random generator
	srand(time(NULL));

	//sanitize, decode and copy the CAN-if name.
	thread_arg.can_if_name=argv[1];
	num_of_pSDAQ = atoi(argv[2]);
	if(!num_of_pSDAQ || num_of_pSDAQ >= Parking_address)
	{
		printf("Amount of pseudo_SDAQ is invalid. Range 1..%d\n",Parking_address-1);
		exit(1);
	}

	//mount signal SIGINT to the signal handler
	signal(SIGINT, handle_sigint);

	CAN_socket_RX_Thread_id = malloc(sizeof(CAN_socket_RX_Thread_id)*num_of_pSDAQ); //allocate memory for the threads tags
	pSDAQs_mem = malloc(sizeof(struct pSDAQ_memory_space)*num_of_pSDAQ); //allocate memory for the pseudo_SDAQ units memory space;
	SDAQs_mem_access = malloc(sizeof(pthread_mutex_t)*num_of_pSDAQ);
	for(int i=0;i<num_of_pSDAQ;i++)
	{
		active_threads++;
		pthread_mutex_init(&SDAQs_mem_access[i], NULL);
		pthread_mutex_lock(&thread_make_lock);
		thread_arg.serial_number=i+1;
		memset(&(pSDAQs_mem[i]), 0, sizeof(struct pSDAQ_memory_space));
		pSDAQs_mem[i].address = Parking_address;
		pSDAQs_mem[i].number_of_channels = 16;
		thread_arg.pSDAQ_mem = &pSDAQs_mem[i];
		pthread_create(&CAN_socket_RX_Thread_id[i], NULL, pseudo_SDAQ, &thread_arg);
	}

	pthread_mutex_lock(&SDAQs_mem_access[0]);
		pSDAQs_mem[0].ch_cal_date[0].amount_of_points=8;
		pSDAQs_mem[0].ch_cal_date[15].amount_of_points=8;
		//pSDAQs_mem[0].number_of_channels = 2;
		pSDAQs_mem[0].data_ref_values[0][0] = 789.321;
		pSDAQs_mem[0].data_mes_values[0][0] = 321.456;
		pSDAQs_mem[0].out_val[0]+=12.55;
	pthread_mutex_unlock(&SDAQs_mem_access[0]);

	while(SDAQ_psim_run)
	{
		/*
		printf("::");
		fflush(stdout);
		scanf("%s",usr_com);
		printf("User input:%s\n",usr_com);
		*/
		//printf("active threads %d\n",active_threads);
		sleep(1);
	}
	for(int i=0;i<num_of_pSDAQ;i++)
		pthread_join(CAN_socket_RX_Thread_id[i], NULL);// wait pseudo_SDAQ thread to end

	free(CAN_socket_RX_Thread_id);
	free(pSDAQs_mem);
	free(SDAQs_mem_access);
	return 0;
}

void * pseudo_SDAQ(void *varg_pt)//Thread function. Act as an pseudo_SDAQ.
{
	struct thread_arguments_passer arg;
	memcpy(&arg, varg_pt, sizeof(arg));//copy *varg_pt to arg (struct thread_arguments_passer)
	pthread_mutex_unlock(&thread_make_lock);//Unlock threading making

	//Variables for Socket CAN
	struct can_frame frame_rx;
	int RX_bytes;
	struct ifreq ifr;
	struct sockaddr_can addr;
	struct can_filter RX_filter;
	sdaq_can_id *can_filter_enc;
	int socket_num;
	//Variables for SDAQ_dev
	sdaq_can_id *id_dec = (sdaq_can_id *)&(frame_rx.can_id);
	sdaq_set_new_addr *set_new_addr_dec = (sdaq_set_new_addr *)frame_rx.data;
	sdaq_calibration_date *cal_date_dec = (sdaq_calibration_date *)frame_rx.data;
	sdaq_calibration_points_data point_enc, *point_dec = (sdaq_calibration_points_data*)frame_rx.data;
	float noise;
	unsigned char raw_meas_cnt=0, in_sync_cnt=0;
	unsigned int status_send_cnt=Stat_ID_Interval;
	unsigned int sync_status_cnt=0;
	unsigned short pseudo_SDAQ_timestamp=0, ref_timestamp=0, loop_time_diff=0;
	short loop_time_diff_acc = TIME_REF; //time_corrector, accumulator for the Time Loop Lock
	//Variables for select
	struct timeval tv;
	fd_set ready_for_read;
	//Time variables
	struct timespec tstart,tend;
	//Return value
	int retval;

	//CAN Socket Opening
	if((socket_num = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
	{
		perror("Error while opening socket");
		exit(1);
	}

	//Link interface name to socket
	strcpy(ifr.ifr_name, arg.can_if_name); // get name from main arguments
	if(ioctl(socket_num, SIOCGIFINDEX, &ifr))
	{
		printf("CANBUS interface name does not exist\n");
		exit(1);
	}

	/*Filter for CAN messages	-- SocketCAN Filters act as: <received_can_id> & mask == can_id & mask*/
	//load filter's can_id member
	can_filter_enc = (sdaq_can_id *)&RX_filter.can_id;//Set encoder to filter.can_id
	memset(can_filter_enc, 0, sizeof(sdaq_can_id));
	can_filter_enc->flags = 4;//set the EFF
	can_filter_enc->protocol_id = PROTOCOL_ID; // Received Messages with protocol_id == PROTOCOL_ID
	can_filter_enc->payload_type = 0x00; //  Received Messages with payload_type & 0x80 == False, aka Master -> SDAQ.
	//load filter's can_mask member
	can_filter_enc = (sdaq_can_id *)&RX_filter.can_mask; //Set encoder to filter.can_mask
	memset(can_filter_enc, 0, sizeof(sdaq_can_id));
	can_filter_enc->flags = 4;//Received only messages with extended ID (29bit)
	can_filter_enc->protocol_id = -1; // Protocol_id field marked for examination
	can_filter_enc->payload_type = 0x80; // + The most significant bit of Payload_type field marked for examination.
	setsockopt(socket_num, SOL_CAN_RAW, CAN_RAW_FILTER, &RX_filter, sizeof(RX_filter));


	// Add timeout option to the CAN Socket
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	setsockopt(socket_num, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

	//Bind CAN Socket to address
	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if(bind(socket_num, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("Error in socket bind");
		exit(1);
	}
	//Send status and info on start
	pthread_mutex_lock(&SDAQs_mem_access[arg.serial_number-1]);
		p_DeviceID_and_status(socket_num, arg.pSDAQ_mem->address, arg.serial_number, arg.pSDAQ_mem->status);
		p_DeviceInfo(socket_num, arg.pSDAQ_mem->address, arg.pSDAQ_mem->number_of_channels);
	pthread_mutex_unlock(&SDAQs_mem_access[arg.serial_number-1]);
	while(SDAQ_psim_run)
	{
		// Get time
		clock_gettime(CLOCK_MONOTONIC_RAW, &tstart);
		/* Set Watch SocketCAN to see when it's available for reading. */
		FD_ZERO(&ready_for_read); //init ready_for_read
		FD_SET(socket_num, &ready_for_read); //link Socket_num with ready_for_read
		tv.tv_sec = 0;
		tv.tv_usec = loop_time_diff_acc * 1000;// timeout of select, ~100ms adjuster in every loop
		//wait socket_num to be ready for read, or expired after timeout
		retval = select(socket_num+1, &ready_for_read, NULL, NULL, &tv);
		if(retval == -1)
		{
			perror("select()");
			close(socket_num);
			pthread_exit(NULL);
		}
		else if(retval)// Socket_num ready to read
		{
			RX_bytes=read(socket_num, &frame_rx, sizeof(frame_rx));
			if(RX_bytes==sizeof(frame_rx))
			{
				pthread_mutex_lock(&SDAQs_mem_access[arg.serial_number-1]);
					if(id_dec->device_addr==arg.pSDAQ_mem->address||id_dec->device_addr==Broadcast)
					{
						switch(id_dec->payload_type)
						{
							case Stop_command:
								arg.pSDAQ_mem->status &= ~(1); //clear run bit of status byte
								p_DeviceID_and_status(socket_num, arg.pSDAQ_mem->address, arg.serial_number, arg.pSDAQ_mem->status);
								break;
							case Start_command:
								arg.pSDAQ_mem->status |= 1; //set run bit of status byte
								p_DeviceID_and_status(socket_num, arg.pSDAQ_mem->address, arg.serial_number, arg.pSDAQ_mem->status);
								break;
							case Configure_Additional_data:
								raw_meas_cnt = frame_rx.data[0];//from white paper
								break;
							case Set_dev_address:
								if(set_new_addr_dec->dev_sn == arg.serial_number && set_new_addr_dec->new_address)
								{
									arg.pSDAQ_mem->address = set_new_addr_dec->new_address;
									p_DeviceID_and_status(socket_num,arg.pSDAQ_mem->address, arg.serial_number, arg.pSDAQ_mem->status);
								}
								else if(!set_new_addr_dec->new_address)
									printf("Error at SDAQ_psim %2d: Invalid address (%d)\n",arg.serial_number,set_new_addr_dec->new_address);
								break;
							case Change_SDAQ_baudrate:
								p_DeviceID_and_status(socket_num, arg.pSDAQ_mem->address, arg.serial_number, arg.pSDAQ_mem->status);
								break;
							case Query_Dev_info:
								p_DeviceID_and_status(socket_num, arg.pSDAQ_mem->address, arg.serial_number, arg.pSDAQ_mem->status);
								p_DeviceInfo(socket_num, arg.pSDAQ_mem->address, arg.pSDAQ_mem->number_of_channels);
								for(int i=0;i<arg.pSDAQ_mem->number_of_channels;i++)
									p_calibration_date(socket_num, arg.pSDAQ_mem->address, i+1, &(arg.pSDAQ_mem->ch_cal_date[i]));
								break;
							case Query_Calibration_Data:
								if(id_dec->device_addr==arg.pSDAQ_mem->address && id_dec->channel_num<=arg.pSDAQ_mem->number_of_channels && id_dec->channel_num)
								{
									for(int j=0;j<8;j++)
									{
										point_enc.data_of_point = arg.pSDAQ_mem->data_ref_values[id_dec->channel_num-1][j];
										point_enc.type = 1;
										point_enc.points_num = j;
										p_calibration_points_data(socket_num, arg.pSDAQ_mem->address, id_dec->channel_num, &point_enc);
										point_enc.data_of_point = arg.pSDAQ_mem->data_mes_values[id_dec->channel_num-1][j];
										point_enc.type = 2;
										p_calibration_points_data(socket_num, arg.pSDAQ_mem->address, id_dec->channel_num, &point_enc);
									}
									p_calibration_date(socket_num, arg.pSDAQ_mem->address, id_dec->channel_num, &(arg.pSDAQ_mem->ch_cal_date[id_dec->channel_num-1]));
								}
								break;
							case Write_calibration_Date:
								if(id_dec->device_addr==arg.pSDAQ_mem->address
								&& id_dec->channel_num<=arg.pSDAQ_mem->number_of_channels
								&& id_dec->channel_num)
								{
									if(cal_date_dec->amount_of_points<=8)
									{
										arg.pSDAQ_mem->ch_cal_date[id_dec->channel_num-1].date = cal_date_dec->date;
										arg.pSDAQ_mem->ch_cal_date[id_dec->channel_num-1].amount_of_points = cal_date_dec->amount_of_points;
									}
								}
								break;
							case Write_calibration_Point_Data:
								if(id_dec->device_addr==arg.pSDAQ_mem->address
								&& id_dec->channel_num<=arg.pSDAQ_mem->number_of_channels
								&& id_dec->channel_num)
								{
									if(point_dec->points_num<8)
									{
										switch(point_dec->type)
										{
											case meas:
												(arg.pSDAQ_mem->data_ref_values[id_dec->channel_num-1][point_dec->points_num]) = point_dec->data_of_point;
												break;
											case ref:
												(arg.pSDAQ_mem->data_mes_values[id_dec->channel_num-1][point_dec->points_num]) = point_dec->data_of_point;
												break;
										}
									}
								}
								break;
							case Synchronization_command:
								if(id_dec->device_addr==Broadcast)
								{
									ref_timestamp = *((unsigned short *)frame_rx.data);
									//printf("reftime = %hu devtime = %hu\n",ref_timestamp,pseudo_SDAQ_timestamp);
									p_debug_data(socket_num, arg.pSDAQ_mem->address, ref_timestamp, pseudo_SDAQ_timestamp);
									if(dev_ref_time_diff_cal(pseudo_SDAQ_timestamp,ref_timestamp) < 100)
									{

										if(in_sync_cnt>1)
										{
											arg.pSDAQ_mem->status |= 1<<In_sync;
											sync_status_cnt=Sync_Status_Interval;
										}
										else
											in_sync_cnt++;
										pseudo_SDAQ_timestamp += dev_ref_time_diff_cal(pseudo_SDAQ_timestamp,ref_timestamp);
									}
									else
									{
										//printf("devtime = reftime\n");
										arg.pSDAQ_mem->status &= ~(1<<In_sync);
										pseudo_SDAQ_timestamp = ref_timestamp;
										in_sync_cnt = 0;
									}
								}
								break;
						}
					}
				pthread_mutex_unlock(&SDAQs_mem_access[arg.serial_number-1]);
			}
		}
		else //select expired from Timeout
		{
			if(arg.pSDAQ_mem->status & 0x01)//check run bit of status byte
			{
				pthread_mutex_lock(&SDAQs_mem_access[arg.serial_number-1]);
					for(int i=0;i<arg.pSDAQ_mem->number_of_channels;i++)
					{
						noise = ((rand()%20)-10)/1000.0;
						p_measure(socket_num, arg.pSDAQ_mem->address, i+1, 0, arg.pSDAQ_mem->out_val[i]+noise, pseudo_SDAQ_timestamp);
						if(raw_meas_cnt >= 10)
							p_measure_raw(socket_num, arg.pSDAQ_mem->address, i+1, 0, arg.pSDAQ_mem->out_val[i]+noise, pseudo_SDAQ_timestamp);
					}
				pthread_mutex_unlock(&SDAQs_mem_access[arg.serial_number-1]);
				if(raw_meas_cnt)
				{
					raw_meas_cnt++;
					if(raw_meas_cnt >= 11)
						raw_meas_cnt=1;
				}
			}
		}
		if(!status_send_cnt) //in every status_send_cnt zero a status message transmitted
		{
			if(!sync_status_cnt) //in every status_send_cnt zero a the sync flag is reset
			 	arg.pSDAQ_mem->status &= ~(1<<In_sync);
			else
				sync_status_cnt--;
			pthread_mutex_lock(&SDAQs_mem_access[arg.serial_number-1]);
				p_DeviceID_and_status(socket_num, arg.pSDAQ_mem->address, arg.serial_number, arg.pSDAQ_mem->status);
			pthread_mutex_unlock(&SDAQs_mem_access[arg.serial_number-1]);
			status_send_cnt = Stat_ID_Interval;
		}
		else
			status_send_cnt--;

		// get time and calc different
		clock_gettime(CLOCK_MONOTONIC_RAW, &tend);
		loop_time_diff = (tend.tv_nsec - tstart.tv_nsec)/1000000;
		loop_time_diff += (tend.tv_sec - tstart.tv_sec)*1000;
		//add time of loop to pseudo_SDAQ_timestamp
		pseudo_SDAQ_timestamp += loop_time_diff;
		if(pseudo_SDAQ_timestamp>=60000)
			pseudo_SDAQ_timestamp -= 60000;
		//calculate new time for loop
		loop_time_diff_acc += TIME_REF - loop_time_diff;
		//printf("Newloop_time_before = %4hi\n", loop_time_diff_acc);
		if(loop_time_diff_acc<0)
			loop_time_diff_acc = 1;
		if(loop_time_diff_acc>=TIME_REF) // lock acc top value to 100 ms
			loop_time_diff_acc = TIME_REF;
		//printf("thread (%2d) Timediff= %4hu Newloop_time= %4hi\n", arg.serial_number, loop_time_diff, loop_time_diff_acc);
	}
	close(socket_num);
	active_threads--;
	return NULL;
}

short dev_ref_time_diff_cal(unsigned short dev_time, unsigned short ref_time)
{
	short ret = dev_time > ref_time ? dev_time - ref_time : ref_time - dev_time;
	if(ret<0)
		ret = 60000 - dev_time - ref_time;
	return ret;
}

void print_usage(char *prog_name)
{
	const char preamp[] = {
	"Program: SDAQ_psim  Copyright (C) 12019-12020  Sam Harry Tzavaras\n"
    "This program comes with ABSOLUTELY NO WARRANTY; for details see LICENSE.\n"
    "This is free software, and you are welcome to redistribute it\n"
    "under certain conditions; for details see LICENSE.\n"
	};
	const char exp[] = {
	"\tCAN-IF: The name of the CAN-Bus adapter\n\n"
	"\tNum_of_pSDAQ: The number of the pseudo_SDAQ devices, range 1..62\n"
	};
	printf("%s\nUsage: %s CAN-IF [Num_of_pSDAQ]\n\n%s\n", preamp, prog_name,exp);
	return;
}
