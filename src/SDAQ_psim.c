/*
Program: SDAQ_psim. A virtual device simulator for SDAQ-CAN Devices.
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
#define VERSION "1.0" /*Release Version of SDAQ_psim*/

#define TIME_REF 100 //loop time ref
#define Stat_ID_Interval 10000/TIME_REF //for 10 sec with base time TIME_REF
#define Sync_Status_Interval 120/(Stat_ID_Interval) //for 120 seconds reset time for In_Sync flag based on Stat_ID_Interval

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

//Include SDAQ Driver header
#include "SDAQ_psim_UI.h" // <-- #include "SDAQ_drv.h" and #include "SDAQ_psim_types.h"
#include "CANif_discovery.h"
#include "ver.h"

//Global variables
unsigned char active_threads=0;
pthread_mutex_t thread_make_lock = PTHREAD_MUTEX_INITIALIZER;


struct thread_arguments_passer
{
	char *can_if_name;
	unsigned int serial_number;
	unsigned int start_sn;
	pSDAQ_memory_space *pSDAQ_mem;
};

void sigint_signal_handler(int signum)
{
	SDAQ_psim_run = 0;
	return;
}

//application functions
void print_usage(char *prog_name);
void print_units(void);
short dev_ref_time_diff_cal(unsigned short dev_time, unsigned short ref_time);
void * pseudo_SDAQ(void *varg_pt);//Thread function. Act as an pseudo_SDAQ.

int main(int argc, char *argv[])
{
	int c;
	unsigned int start_sn = 1;
	unsigned char num_of_pSDAQ, init_num_of_channels = 1;
	struct winsize term_init_size;
	//variables for threads
	pthread_t *CAN_socket_RX_Thread_id;
	struct thread_arguments_passer thread_arg;
	pSDAQ_memory_space *pSDAQs_mem;

	if(argc == 1)
	{
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	opterr = 1;
	while ((c = getopt (argc, argv, "hvls:c:u")) != -1)
	{
		switch (c)
		{
			case 'h'://help
				print_usage(argv[0]);
				exit(EXIT_SUCCESS);
			case 'v'://Version
				printf("Release: %s (%s)\nCompile Date: %s\nVer: "VERSION"\n", get_curr_git_hash(), get_release_date(), get_compile_date());
				exit(EXIT_SUCCESS);
			case 'l'://List of CAN-IF
				CANif_discovery();
				exit(EXIT_SUCCESS);
			case 's'://Start S/N
				start_sn = atoi(optarg);
				break;
			case 'c'://Start amount of channels
				init_num_of_channels = atoi(optarg);
				if(!init_num_of_channels || init_num_of_channels>SDAQ_MAX_AMOUNT_OF_CHANNELS)
				{
					printf("Amount of channels argument is invalid\n");
					exit(EXIT_FAILURE);
				}
				break;
			case 'u':
				print_units();
				exit(EXIT_SUCCESS);
				break;
			case '?':
				//print_usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	if(argv[optind]==NULL || argv[optind+1]==NULL)
	{
		printf("!!! CAN-IF and/or Num_of_pSDAQ argument Missing !!!\n");
		exit(EXIT_FAILURE);
	}
	if(strlen(argv[optind]) >= IFNAMSIZ)//Check length of CAN-if name
	{
		fprintf(stderr, "CAN-IF name too big (>=%d)\n", IFNAMSIZ);
		exit(EXIT_FAILURE);
	}
	//init pseudo random generator
	srand(time(NULL));

	//sanitize, decode and copy the CAN-if name.
	thread_arg.can_if_name=argv[optind];
	num_of_pSDAQ = atoi(argv[optind+1]);
	if(!num_of_pSDAQ || num_of_pSDAQ >= Parking_address)
	{
		printf("Amount of pseudo_SDAQ is invalid. Range 1..%d\n",Parking_address-1);
		exit(1);
	}
	//Link signal SIGINT to quit_signal_handler
	signal(SIGINT, sigint_signal_handler);

	//Allocation of memory
	CAN_socket_RX_Thread_id = malloc(sizeof(CAN_socket_RX_Thread_id)*num_of_pSDAQ); //allocate memory for the threads tags
	pSDAQs_mem = malloc(sizeof(pSDAQ_memory_space)*num_of_pSDAQ); //allocate memory for the pseudo_SDAQs
	SDAQs_mem_access = malloc(sizeof(pthread_mutex_t)*num_of_pSDAQ);
	//Call and start threads
	for(int i=0;i<num_of_pSDAQ;i++)
	{
		active_threads++;
		pthread_mutex_init(&SDAQs_mem_access[i], NULL);
		pthread_mutex_lock(&thread_make_lock);
		thread_arg.serial_number = i+start_sn;
		thread_arg.start_sn = start_sn;
		memset(&(pSDAQs_mem[i]), 0, sizeof(pSDAQ_memory_space));
		pSDAQs_mem[i].address = Parking_address;
		pSDAQs_mem[i].number_of_channels = init_num_of_channels;
		thread_arg.pSDAQ_mem = &pSDAQs_mem[i];
		pthread_create(&CAN_socket_RX_Thread_id[i], NULL, pseudo_SDAQ, &thread_arg);
	}
	//Run user's interface (ncurses)
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &term_init_size);// get current size of terminal window
	//Check if the terminal have the minimum size for the application
	if(term_init_size.ws_col<100 || term_init_size.ws_row<35)
		printf("Terminal need to be at least 100X35 Characters to run shell\n The SDAQ_psim forced to run Headless\n");
	else
		user_interface(thread_arg.can_if_name, start_sn, num_of_pSDAQ, pSDAQs_mem);

	for(int i=0;i<num_of_pSDAQ;i++)
	{
		pthread_join(CAN_socket_RX_Thread_id[i], NULL);// wait pseudo_SDAQ thread to end
		pthread_detach(CAN_socket_RX_Thread_id[i]);//deallocate pseudo_SDAQ thread memory
	}
	free(CAN_socket_RX_Thread_id);
	free(pSDAQs_mem);
	free(SDAQs_mem_access);
	return EXIT_SUCCESS;
}

void * pseudo_SDAQ(void *varg_pt)//Thread function. Act as an pseudo_SDAQ.
{
	struct thread_arguments_passer arg;
	memcpy(&arg, varg_pt, sizeof(arg));//copy *varg_pt to arg (struct thread_arguments_passer)

	//Variables for Socket CAN
	struct can_frame frame_rx;
	int RX_bytes;
	struct ifreq ifr = {0};
	struct sockaddr_can addr = {0};
	struct can_filter RX_filter = {0};
	sdaq_can_id *can_filter_enc;
	int socket_num;
	//Variables for SDAQ_dev
	sdaq_can_id *id_dec = (sdaq_can_id *)&(frame_rx.can_id);
	sdaq_set_new_addr *set_new_addr_dec = (sdaq_set_new_addr *)frame_rx.data;
	sdaq_calibration_date *cal_date_dec = (sdaq_calibration_date *)frame_rx.data;
	sdaq_calibration_points_data point_enc, *point_dec = (sdaq_calibration_points_data*)frame_rx.data;
	float noise;
	unsigned char raw_meas_cnt=0, in_sync_cnt=0;
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
	//Send status and info on start and init status send counter
	pthread_mutex_lock(&SDAQs_mem_access[arg.serial_number-arg.start_sn]);
		p_DeviceID_and_status(socket_num, arg.pSDAQ_mem->address, arg.serial_number, arg.pSDAQ_mem->status);
		arg.pSDAQ_mem->status_send_cnt=Stat_ID_Interval;
	pthread_mutex_unlock(&SDAQs_mem_access[arg.serial_number-arg.start_sn]);
	//Unlock threading making
	pthread_mutex_unlock(&thread_make_lock);
	while(SDAQ_psim_run)
	{
		// Get time
		clock_gettime(CLOCK_MONOTONIC_RAW, &tstart);
		/* Set Watch SocketCAN to see when it's available for reading. */
		FD_ZERO(&ready_for_read); //init ready_for_read
		FD_SET(socket_num, &ready_for_read); //link Socket_num with ready_for_read
		tv.tv_sec = 0;
		tv.tv_usec = loop_time_diff_acc * 1000;// timeout of select, ~100ms adjuster in every loop
		if(!(arg.pSDAQ_mem->pSDAQ_flags&(1<<disable)))
		{
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
					pthread_mutex_lock(&SDAQs_mem_access[arg.serial_number-arg.start_sn]);
						if(id_dec->device_addr==arg.pSDAQ_mem->address||id_dec->device_addr==Broadcast)
						{
							switch(id_dec->payload_type)
							{
								case Stop_command:
									arg.pSDAQ_mem->status &= ~(1); //clear run bit of status byte, stop measure
									p_DeviceID_and_status(socket_num, arg.pSDAQ_mem->address, arg.serial_number, arg.pSDAQ_mem->status);
									break;
								case Start_command:
									if(arg.pSDAQ_mem->address != Parking_address)
									{
										arg.pSDAQ_mem->status |= 1; //set run bit of status byte, start measure
										p_DeviceID_and_status(socket_num, arg.pSDAQ_mem->address, arg.serial_number, arg.pSDAQ_mem->status);
									}
									break;
								case Configure_Additional_data:
									raw_meas_cnt = frame_rx.data[0];//from white paper
									break;
								case Set_dev_address:
									if(set_new_addr_dec->dev_sn == arg.serial_number)
									{
										if(!set_new_addr_dec->new_address)
											fprintf(stderr, "Error at SDAQ_psim %2d: Invalid address (%d)\n",arg.serial_number,set_new_addr_dec->new_address);
										else if(set_new_addr_dec->new_address<=Parking_address)
										{
											arg.pSDAQ_mem->status &= ~(1); //clear run bit of status byte, stop measure
											arg.pSDAQ_mem->address = set_new_addr_dec->new_address;
											p_DeviceID_and_status(socket_num, arg.pSDAQ_mem->address, arg.serial_number, arg.pSDAQ_mem->status);
										}
									}
									break;
								case Change_SDAQ_baudrate:
									arg.pSDAQ_mem->status &= ~(1); //clear run bit of status byte, stop measure
									p_DeviceID_and_status(socket_num, arg.pSDAQ_mem->address, arg.serial_number, arg.pSDAQ_mem->status);
									break;
								case Query_Dev_info:
									p_DeviceID_and_status(socket_num, arg.pSDAQ_mem->address, arg.serial_number, arg.pSDAQ_mem->status);
									p_DeviceInfo(socket_num, arg.pSDAQ_mem->address, arg.pSDAQ_mem->number_of_channels);
									for(int i=0; i<arg.pSDAQ_mem->number_of_channels; i++)
										p_calibration_date(socket_num, arg.pSDAQ_mem->address, i+1, &(arg.pSDAQ_mem->ch_cal_date[i]));
									break;
								case Query_Calibration_Data:
									if(id_dec->device_addr==arg.pSDAQ_mem->address &&
									   id_dec->channel_num<=arg.pSDAQ_mem->number_of_channels &&
									   id_dec->channel_num)
									{
										for(int j=0;j<MAX_AMOUNT_OF_POINTS;j++)
										{
											for(int k=0; k<MAX_DATA_ON_POINT; k++)
											{
												point_enc.data_of_point = arg.pSDAQ_mem->data_cal_values[id_dec->channel_num-1][j][k];
												point_enc.type = k+1;
												point_enc.points_num = j;
												p_calibration_points_data(socket_num, arg.pSDAQ_mem->address, id_dec->channel_num, &point_enc);
											}
										}
										p_calibration_date(socket_num, arg.pSDAQ_mem->address, id_dec->channel_num, &(arg.pSDAQ_mem->ch_cal_date[id_dec->channel_num-1]));
									}
									break;
								case Write_calibration_Date:
									if(id_dec->device_addr==arg.pSDAQ_mem->address
									&& id_dec->channel_num<=arg.pSDAQ_mem->number_of_channels
									&& id_dec->channel_num)
									{
										if(cal_date_dec->amount_of_points<=SDAQ_MAX_AMOUNT_OF_CHANNELS)
											memcpy(&(arg.pSDAQ_mem->ch_cal_date[id_dec->channel_num-1]), cal_date_dec, sizeof(sdaq_calibration_date));
									}
									break;
								case Write_calibration_Point_Data:
									if(id_dec->device_addr==arg.pSDAQ_mem->address
									&& id_dec->channel_num<=arg.pSDAQ_mem->number_of_channels
									&& id_dec->channel_num)
									{
										if(!(arg.pSDAQ_mem->status&1)||!(arg.pSDAQ_mem->ch_cal_date[id_dec->channel_num-1].amount_of_points))
										{
											if(point_dec->points_num<MAX_AMOUNT_OF_POINTS && point_dec->type && point_dec->type<=MAX_DATA_ON_POINT)//Sanitization according to whitepaper.
											{
												arg.pSDAQ_mem->data_cal_values[id_dec->channel_num-1]
																			  [point_dec->points_num]
																			  [point_dec->type-1] = point_dec->data_of_point;
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
											arg.pSDAQ_mem->status &= ~(1<<In_sync);
											pseudo_SDAQ_timestamp = ref_timestamp;
											in_sync_cnt = 0;
										}
									}
									break;
							}
						}
					pthread_mutex_unlock(&SDAQs_mem_access[arg.serial_number-arg.start_sn]);
				}
			}
			else //select expired from Timeout
			{
				if(arg.pSDAQ_mem->status & 0x01)//check run bit of status byte
				{
					pthread_mutex_lock(&SDAQs_mem_access[arg.serial_number-arg.start_sn]);
						for(int i=0;i<arg.pSDAQ_mem->number_of_channels;i++)
						{
							noise = arg.pSDAQ_mem->noise & (1<<i) ? ((rand()%20)-10)/1000.0 : 0;
							p_measure(socket_num, arg.pSDAQ_mem->address, i+1, (((arg.pSDAQ_mem->nosensor)>>i)&1)|((((arg.pSDAQ_mem->out_of_range)>>i)&1)<<1)|((((arg.pSDAQ_mem->over_range)>>i)&1)<<2),
																				 arg.pSDAQ_mem->ch_cal_date[i].cal_units,
																				 arg.pSDAQ_mem->out_val[i]+noise, pseudo_SDAQ_timestamp);
							if(raw_meas_cnt >= 10)
								p_measure_raw(socket_num, arg.pSDAQ_mem->address, i+1, (arg.pSDAQ_mem->nosensor>>i)&1,
																						arg.pSDAQ_mem->out_val[i]+noise, pseudo_SDAQ_timestamp);
						}
					pthread_mutex_unlock(&SDAQs_mem_access[arg.serial_number-arg.start_sn]);
					if(raw_meas_cnt)
					{
						raw_meas_cnt++;
						if(raw_meas_cnt >= 11)
							raw_meas_cnt=1;
					}
				}
			}
			pthread_mutex_lock(&SDAQs_mem_access[arg.serial_number-arg.start_sn]);
				if(!arg.pSDAQ_mem->status_send_cnt) //in every status_send_cnt zero a status message transmitted
				{
					if(!sync_status_cnt) //in every status_send_cnt zero the sync flag is reset
					 	arg.pSDAQ_mem->status &= ~(1<<In_sync);
					else
						sync_status_cnt--;
					p_DeviceID_and_status(socket_num, arg.pSDAQ_mem->address, arg.serial_number, arg.pSDAQ_mem->status);
					arg.pSDAQ_mem->status_send_cnt = Stat_ID_Interval;
				}
				arg.pSDAQ_mem->status_send_cnt--;
				if(arg.pSDAQ_mem->pSDAQ_flags & 1<<info_send)
				{
					arg.pSDAQ_mem->pSDAQ_flags &= ~(1<<info_send);
					p_DeviceInfo(socket_num, arg.pSDAQ_mem->address, arg.pSDAQ_mem->number_of_channels);
				}
				if(arg.pSDAQ_mem->pSDAQ_flags & 1<<cal_dates_send)//check if the force send of the cal dates flag is on.
				{
					arg.pSDAQ_mem->pSDAQ_flags &= ~(1<<cal_dates_send); //reset force send of the cal dates flag
					for(int i=0;i<arg.pSDAQ_mem->number_of_channels;i++)
						p_calibration_date(socket_num, arg.pSDAQ_mem->address, i+1, &(arg.pSDAQ_mem->ch_cal_date[i]));
				}
			pthread_mutex_unlock(&SDAQs_mem_access[arg.serial_number-arg.start_sn]);
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
			if(loop_time_diff_acc<0)
				loop_time_diff_acc = 1;
			if(loop_time_diff_acc>=TIME_REF) // lock acc top value to 100 ms
				loop_time_diff_acc = TIME_REF;
		}
		else
			usleep(TIME_REF * 1000);
	}
	close(socket_num);
	active_threads--;
	//printf("Thread of pseudoSDAQ with S/N:%2d Exit...\n",arg.serial_number);
	return NULL;
}

short dev_ref_time_diff_cal(unsigned short dev_time, unsigned short ref_time)
{
	short ret = dev_time > ref_time ? dev_time - ref_time : ref_time - dev_time;
	if(ret<0)
		ret = 60000 - dev_time - ref_time;
	return ret;
}

void print_units(void)
{
	printf("{\"Base_offset\":%d,\"SDAQ_UNITs\":[", Unit_code_base_region_size);
	for(int i=Unit_code_base_region_size; unit_str[i];i++)
	{
		printf("\"%s\"",unit_str[i]);
		if(unit_str[i+1])
			printf(",");
	}
	printf("]}\n");
}

void print_usage(char *prog_name)
{
	const char preamp[] = {
	"Program: SDAQ_psim  Copyright (C) 12019-12021  Sam Harry Tzavaras\n"
    "This program comes with ABSOLUTELY NO WARRANTY; for details see LICENSE.\n"
    "This is free software, and you are welcome to redistribute it\n"
    "under certain conditions; for details see LICENSE.\n"
	};
	const char exp[] = {
	"\tCAN-IF: The name of the CAN-Bus interface\n\n"
	"\tNum_of_pSDAQ: The number of the pseudo_SDAQ devices, Range 1..62\n\n"
	"\tOptions:\n"
	"\t         -h : Print Help\n"
	"\t         -v : Print Version\n"
	"\t         -l : Print list of CAN-IFs\n"
	"\t         -s : S/N of the first pseudo_SDAQ. (Default 1)\n"
	"\t         -c : Initial Amount of channels of each pseudo_SDAQ, (default 1, Range:[1-16..63])\n"
	"\t			-u : Print SDAQ UNITs as JSON table"
	};
	printf("%s\nUsage: %s CAN-IF Num_of_pSDAQ [Options]\n\n%s\n%s", preamp, prog_name, exp, shell_help_str);
	return;
}

