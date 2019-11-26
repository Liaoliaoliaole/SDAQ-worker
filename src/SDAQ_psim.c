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
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <ncurses.h>
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
#include "SDAQ_drv.h"

//struct definition of memory space of a pseudo_SDAQ
struct pSDAQ_memory_space
{
	unsigned short noise;
	unsigned short nosensor;
	unsigned char status;
	unsigned char address;
	unsigned char number_of_channels;
	float out_val[16];
	sdaq_calibration_date ch_cal_date[16];
	float data_cal_values[16][16][6];
};

//Global variables
unsigned char SDAQ_psim_run=1, active_threads=0;
pthread_mutex_t thread_make_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t *SDAQs_mem_access;

struct thread_arguments_passer
{
	char *can_if_name;
	unsigned int serial_number;
	unsigned int start_sn;
	struct pSDAQ_memory_space *pSDAQ_mem;
};

void sigint_signal_handler(int signum)
{
	SDAQ_psim_run = 0;
	return;
}

//application functions
void print_usage(char *prog_name);
void user_interface(unsigned int start_sn, unsigned char num_of_pSDAQ, struct pSDAQ_memory_space *pSDAQs_mem);
short dev_ref_time_diff_cal(unsigned short dev_time, unsigned short ref_time);
void * pseudo_SDAQ(void *varg_pt);//Thread function. Act as an pseudo_SDAQ.

int main(int argc, char *argv[])
{
	unsigned int start_sn;
	unsigned char num_of_pSDAQ;
	struct winsize term_init_size;
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
	//Link signal SIGINT to quit_signal_handler
	signal(SIGINT, sigint_signal_handler);
	//Check if user have enter serial number start. If yes use it otherwise from 1
	start_sn = !argv[3] ? 1 : atoi(argv[3]);
	start_sn = start_sn ? start_sn : 1;
	//Allocation of memory
	CAN_socket_RX_Thread_id = malloc(sizeof(CAN_socket_RX_Thread_id)*num_of_pSDAQ); //allocate memory for the threads tags
	pSDAQs_mem = malloc(sizeof(struct pSDAQ_memory_space)*num_of_pSDAQ); //allocate memory for the pseudo_SDAQs
	SDAQs_mem_access = malloc(sizeof(pthread_mutex_t)*num_of_pSDAQ);
	//Call and start threads
	for(int i=0;i<num_of_pSDAQ;i++)
	{
		active_threads++;
		pthread_mutex_init(&SDAQs_mem_access[i], NULL);
		pthread_mutex_lock(&thread_make_lock);
		thread_arg.serial_number = i+start_sn;
		thread_arg.start_sn = start_sn;
		memset(&(pSDAQs_mem[i]), 0, sizeof(struct pSDAQ_memory_space));
		pSDAQs_mem[i].address = Parking_address;
		pSDAQs_mem[i].number_of_channels = 16;
		thread_arg.pSDAQ_mem = &pSDAQs_mem[i];
		pthread_create(&CAN_socket_RX_Thread_id[i], NULL, pseudo_SDAQ, &thread_arg);
	}
	//Run user's interface (ncurses)
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &term_init_size);// get current size of terminal window
	//Check if the terminal have the minimum size for the application
	if(term_init_size.ws_col<110 || term_init_size.ws_row<33)
		printf("Terminal need to be at least 110X33 Characters to run shell\n The SDAQ_psim forced to run head-less\n");
	else
		user_interface(start_sn, num_of_pSDAQ, pSDAQs_mem);

	for(int i=0;i<num_of_pSDAQ;i++)
		pthread_join(CAN_socket_RX_Thread_id[i], NULL);// wait pseudo_SDAQ thread to end

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
	pthread_mutex_lock(&SDAQs_mem_access[arg.serial_number-arg.start_sn]);
		p_DeviceID_and_status(socket_num, arg.pSDAQ_mem->address, arg.serial_number, arg.pSDAQ_mem->status);
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
								arg.pSDAQ_mem->status &= ~(1); //clear run bit of status byte
								p_DeviceID_and_status(socket_num, arg.pSDAQ_mem->address, arg.serial_number, arg.pSDAQ_mem->status);
								break;
							case Start_command:
								if(arg.pSDAQ_mem->address != Parking_address)
								{
									arg.pSDAQ_mem->status |= 1; //set run bit of status byte
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
										//printf("SDAQ_psim %2d: New address: %2d\n",arg.serial_number,set_new_addr_dec->new_address);
										arg.pSDAQ_mem->address = set_new_addr_dec->new_address;
										p_DeviceID_and_status(socket_num, arg.pSDAQ_mem->address, arg.serial_number, arg.pSDAQ_mem->status);
									}
								}
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
								if(id_dec->device_addr==arg.pSDAQ_mem->address &&
								   id_dec->channel_num<=arg.pSDAQ_mem->number_of_channels &&
								   id_dec->channel_num)
								{
									for(int j=0;j<16;j++)
									{
										for(int k=0; k<6; k++)
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
									if(cal_date_dec->amount_of_points<=16)
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
									if(point_dec->points_num<16)
									{
										arg.pSDAQ_mem->data_cal_values[id_dec->channel_num-1]
																	  [point_dec->points_num]
																	  [point_dec->type] = point_dec->data_of_point;
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
						p_measure(socket_num, arg.pSDAQ_mem->address, i+1, ((arg.pSDAQ_mem->nosensor)>>i)&1, arg.pSDAQ_mem->ch_cal_date[i].cal_units,
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
		if(!status_send_cnt) //in every status_send_cnt zero a status message transmitted
		{
			if(!sync_status_cnt) //in every status_send_cnt zero a the sync flag is reset
			 	arg.pSDAQ_mem->status &= ~(1<<In_sync);
			else
				sync_status_cnt--;
			pthread_mutex_lock(&SDAQs_mem_access[arg.serial_number-arg.start_sn]);
				p_DeviceID_and_status(socket_num, arg.pSDAQ_mem->address, arg.serial_number, arg.pSDAQ_mem->status);
			pthread_mutex_unlock(&SDAQs_mem_access[arg.serial_number-arg.start_sn]);
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
	printf("Thread of pseudoSDAQ with S/N:%2d Exit...\n",arg.serial_number);
	return NULL;
}

short dev_ref_time_diff_cal(unsigned short dev_time, unsigned short ref_time)
{
	short ret = dev_time > ref_time ? dev_time - ref_time : ref_time - dev_time;
	if(ret<0)
		ret = 60000 - dev_time - ref_time;
	return ret;
}

const char shell_help_str[]={
	"\t\t\t      -----SDAQ_psim Shell-----\n"
	"\n KEYS:\n"
	"\tKEY_UP    = Buffer up\n"
	"\tKEY_DOWN  = Buffer Down\n"
	"\tKEY_LEFT  = Cursor move left by 1\n"
	"\tKEY_RIGTH = Cursor move Right by 1\n"
	"\tCtrl + C  = Clear current buffer\n"
	"\tCtrl + L  = Clear screen\n"
	"\tCtrl + Q  = Quit\n"
	"\n COMMANDS:\n"
	"\tstatus = Print a list of with status from all the pseudo-SDAQs\n"
	"\tstatus [pseudo-SDAQ S/N] = Print a list with status of the specified pseudo-SDAQ\n"
	"\tget (pseudo-SDAQ S/N) = Get the current state of the pseudo-SDAQ\n"
	"\tset (pseudo-SDAQ S/N) (ch# || all) noise = Set pseudo-random noise on channel(s)\n"
	"\tset (pseudo-SDAQ S/N) (ch# || all) nonoise = Remove noise from channel(s)\n"
	"\tset (pseudo-SDAQ S/N) (ch# || all) sensor = Reset No sensor flag(s)\n"
	"\tset (pseudo-SDAQ S/N) (ch# || all) nosensor = Set No sensor flag(s)\n"
	"\tset (pseudo-SDAQ S/N) (ch# || all) value.num = Write value to Channel(s) output\n"
	"\tset (pseudo-SDAQ S/N) addr (new_address_# || parking) = Set pseudo-SDAQ address\n"
	"\tset (pseudo-SDAQ S/N) amount  = Set pseudo-SDAQ amount of channels. Range 1..16\n"
};


void print_usage(char *prog_name)
{
	const char preamp[] = {
	"Program: SDAQ_psim  Copyright (C) 12019-12020  Sam Harry Tzavaras\n"
    "This program comes with ABSOLUTELY NO WARRANTY; for details see LICENSE.\n"
    "This is free software, and you are welcome to redistribute it\n"
    "under certain conditions; for details see LICENSE.\n"
	};
	const char exp[] = {
	"\tCAN-IF: The name of the CAN-Bus interface\n\n"
	"\tNum_of_pSDAQ: The number of the pseudo_SDAQ devices, Range 1..62\n\n"
	"\tS/N_start_Num: (Optional) The S/N of first pSDAQ. (Default 1)\n"
	};
	printf("%s\nUsage: %s CAN-IF Num_of_pSDAQ [S/N_start_Num]\n\n%s\n%s", preamp, prog_name, exp, shell_help_str);
	return;
}

#define user_inp_buf_size 80
#define max_amount_of_user_arg 20

//function for decode user input
int user_inp_dec(char **argv, char *usr_in_buff, unsigned int start_sn, unsigned char num_of_pSDAQ, struct pSDAQ_memory_space *pSDAQs_mem);
//function for execution of user's command input
void user_com(unsigned int argc, char **argv, unsigned int start_sn, unsigned char num_of_pSDAQ, struct pSDAQ_memory_space *pSDAQs_mem);
//SDAQ_psim shell help
void shell_help();

//Implementation of the user's Interface function
void user_interface(unsigned int start_sn, unsigned char num_of_pSDAQ, struct pSDAQ_memory_space *pSDAQs_mem)
{
	unsigned int end_index=0, cur_pos=0, key, argc, last_curx;
	char usr_in_buff[user_inp_buf_size] = {'\0'};
	char *argv[max_amount_of_user_arg] = {NULL};

	initscr(); // start the ncurses mode
	noecho();//disable echo
	raw();//getch without return
	keypad(stdscr, TRUE);
	scrollok(stdscr, TRUE);
	printw("press '?' for help.\n");
	printw("][ ");
	while(SDAQ_psim_run)
	{
		key = getch();// get the user's entrance
		switch(key)
		{
			case 17 ://ctrl + q
				SDAQ_psim_run = 0;
				break;
			case 12 : //ctrl + l
				clear();
				printw("][ %s",usr_in_buff);
				cur_pos = end_index;
				break;
			case KEY_UP:
				move(getcury(stdscr),3);
				clrtoeol();
				printw("%s",usr_in_buff);
				cur_pos = end_index;
				break;
			case KEY_DOWN:
				move(getcury(stdscr),3);
				clrtoeol();
				printw("%s",usr_in_buff);
				cur_pos = end_index;
				break;
			case KEY_LEFT:
				if(cur_pos)
				{
					move(getcury(stdscr),getcurx(stdscr)-1);
					cur_pos--;
				}
				break;
			case KEY_RIGHT:
				if(cur_pos<end_index)
				{
					move(getcury(stdscr),getcurx(stdscr)+1);
					cur_pos++;
				}
				break;
			case KEY_BACKSPACE :
				if(cur_pos)
				{
					for(int i=cur_pos-1;i<=end_index;i++)
						usr_in_buff[i] = usr_in_buff[i+1];
					move(getcury(stdscr),getcurx(stdscr)-1);//move cursor one left
					clrtoeol(); //clear from buffer to the end of line
					end_index--;
					cur_pos--;
					usr_in_buff[end_index] = '\0';
					printw("%s", usr_in_buff + cur_pos);
					move(getcury(stdscr),getcurx(stdscr)-(end_index-cur_pos));
				}
				break;
			case KEY_DC ://Delete key
				if(cur_pos<end_index)
				{
					for(int i=cur_pos;i<=end_index;i++)
						usr_in_buff[i] = usr_in_buff[i+1];
					end_index--;
					clrtoeol();
					printw("%s", usr_in_buff + cur_pos);
					move(getcury(stdscr),getcurx(stdscr)-(end_index-cur_pos));
					usr_in_buff[end_index] = '\0';
				}
				break;
			case KEY_HOME ://Home key
				cur_pos = 0;
				move(getcury(stdscr),3);
				break;
			case KEY_END ://End key
				cur_pos = end_index;
				move(getcury(stdscr),3+end_index);
				break;
			case 3 ://ctrl + c clear buffer
				move(getcury(stdscr),3);
				clrtoeol();
				end_index = 0;
				cur_pos = 0;
				for(int i=0;i<user_inp_buf_size;i++)
					usr_in_buff[i] = '\0';
				break;
			case '\r' :
			case '\n' ://return or enter : Command decode and execution
				usr_in_buff[end_index] = '\0';
				argc = user_inp_dec(argv, usr_in_buff, start_sn, num_of_pSDAQ, pSDAQs_mem);
				user_com(argc, argv, start_sn, num_of_pSDAQ, pSDAQs_mem);
				printw("\n][ ");
				end_index = 0;
				cur_pos = 0;
				for(int i=0;i<user_inp_buf_size;i++)
					usr_in_buff[i] = '\0';
				break;
			case '?' : //user request for help
				last_curx = getcurx(stdscr);
				shell_help();
				refresh();
				clear();
				printw("][ %s",usr_in_buff);
				move(0,last_curx);
				break;
			default : //normal key press
				if(isprint(key))
				{
					if(end_index<user_inp_buf_size-1)
					{	//check if cursor has moved from the user
						if(cur_pos<end_index)
						{	//roll right side of the buffer by one postition
							for(int i=end_index; i>=cur_pos && i>=0; i--)
								usr_in_buff[i+1] = usr_in_buff[i];
						}
						usr_in_buff[cur_pos] = key; // add new pressed key to the buffer
						end_index++;
						printw("%s", usr_in_buff+cur_pos);
						cur_pos++;
						move(getcury(stdscr),getcurx(stdscr)-(end_index-cur_pos));
					}
				}
				//else
					//printw("\ncontrol key = %d\n",key);
				break;
		}
	}
	endwin();
	return;
}

int user_inp_dec(char **arg, char *usr_in_buff, unsigned int start_sn, unsigned char num_of_pSDAQ, struct pSDAQ_memory_space *pSDAQs_mem)
{
	unsigned char i=0;
	arg[i] = strtok (usr_in_buff," ");
	while (arg[i] != NULL)
	{
		i++;
		arg[i] = strtok (NULL, " ");
	}
	return i;
}

int exp_date_dec_validator(struct tm *exp_date_dec, char *buff)
{
	char *buff_arr[2];
	memset(exp_date_dec,0,sizeof(struct tm));
	buff_arr[0] = strtok (buff, "/");
	buff_arr[1] = strtok (NULL, "/");
	if(buff_arr[0] && buff_arr[1])
	{
		if(atoi(buff_arr[0])<1900 || (atoi(buff_arr[1])>11 && atoi(buff_arr[1])))
			return 0;
		exp_date_dec->tm_year = atoi(buff_arr[0]) - 1900;
		exp_date_dec->tm_mon = atoi(buff_arr[1]) - 1;
	}
	return 0;
}

void user_com(unsigned int argc, char **argv, unsigned int start_sn, unsigned char num_of_pSDAQ, struct pSDAQ_memory_space *pSDAQs_mem)
{
	unsigned char sn_dec, channel_dec;
	char *channel_str, str_buff[30];
	time_t date;
	if(argv[0])
	{
		if(!strcmp(argv[0],"status"))
		{
			if(!argv[1])
			{
				for(int i=0; i<num_of_pSDAQ; i++)
				{
					pthread_mutex_lock(&SDAQs_mem_access[i]);
						printw("\n   SDAQ %010d: Addr =",i+start_sn);
						if(pSDAQs_mem[i].address < Parking_address)
							printw(" %2d,",pSDAQs_mem[i].address);
						else
							printw(" Park,");
						printw(" %2d channels,",pSDAQs_mem[i].number_of_channels);
						printw(" %s,",pSDAQs_mem[i].status&0x01?"Measuring":"Stand-By");
						printw(" %sSync",pSDAQs_mem[i].status&(1<<In_sync)?"in":"no");
					pthread_mutex_unlock(&SDAQs_mem_access[i]);
				}
				return;
			}
			else
			{
				sn_dec = atoi(argv[1]);
				if(sn_dec >= start_sn && sn_dec <= start_sn + num_of_pSDAQ-1)
				{
					pthread_mutex_lock(&SDAQs_mem_access[sn_dec - start_sn]);
						printw("\n   SDAQ %010d: Addr=",sn_dec);
						if(pSDAQs_mem[sn_dec - start_sn].address < Parking_address)
							printw("  %2d,",pSDAQs_mem[sn_dec - start_sn].address);
						else
							printw("Park,");
						printw(" %2d channels,",pSDAQs_mem[sn_dec - start_sn].number_of_channels);
						printw(" %s,",pSDAQs_mem[sn_dec - start_sn].status&0x01?"Measuring":"Stand-By");
						printw(" %sSync",pSDAQs_mem[sn_dec - start_sn].status&(1<<In_sync)?"in":"no");
						for(int i=0;i<pSDAQs_mem[sn_dec - start_sn].number_of_channels;i++)
						{
							date = pSDAQs_mem[sn_dec - start_sn].ch_cal_date[i].date;
							strftime (str_buff, sizeof(str_buff),"%Y/%m",gmtime(&date));
							printw("\n\tCH%02d: Expired @ %s, Calibrated with %d point, unit -> %s"
									,i+1
									,str_buff
									,pSDAQs_mem[sn_dec-start_sn].ch_cal_date[i].amount_of_points
									,unit_str[pSDAQs_mem[sn_dec-start_sn].ch_cal_date[i].cal_units]);
						}
					pthread_mutex_unlock(&SDAQs_mem_access[sn_dec - start_sn]);
					return;
				}
			}
		}
		if(!strcmp(argv[0],"get"))
		{
			if(argv[1])
			{
				sn_dec = atoi(argv[1]);//serial number of pseudoSDAQ
				if(sn_dec >= start_sn && sn_dec <= start_sn + num_of_pSDAQ-1)
				{
					pthread_mutex_lock(&SDAQs_mem_access[sn_dec - start_sn]);
						printw("\n   SDAQ %010d: Addr =",sn_dec);
						if(pSDAQs_mem[sn_dec - start_sn].address < Parking_address)
							printw(" %2d,",pSDAQs_mem[sn_dec - start_sn].address);
						else
							printw(" Park,");
						printw(" %2d channels,",pSDAQs_mem[sn_dec - start_sn].number_of_channels);
						printw(" %s,",pSDAQs_mem[sn_dec - start_sn].status&0x01?"Measuring":"Stand-By");
						printw(" %sSync",pSDAQs_mem[sn_dec - start_sn].status&(1<<In_sync)?"in":"no");
						for(int i=0;i<pSDAQs_mem[sn_dec - start_sn].number_of_channels;i++)
						{
							printw("\n\tCH%02d: Out_val = %.3f %s%s%s"
									  ,i+1
									  ,pSDAQs_mem[sn_dec-start_sn].out_val[i]
									  ,unit_str[pSDAQs_mem[sn_dec-start_sn].ch_cal_date[i].cal_units]
									  ,pSDAQs_mem[sn_dec-start_sn].noise & (1<<i) ? ", With Noise":""
									  ,pSDAQs_mem[sn_dec-start_sn].nosensor & (1<<i) ? ", No sensor":"");
						}
					pthread_mutex_unlock(&SDAQs_mem_access[sn_dec - start_sn]);
					return;
				}
			}
		}
		else if(!strcmp(argv[0],"set"))
		{
			if(argv[1])
			{
				sn_dec = atoi(argv[1]);//serial number of pseudoSDAQ
				if(sn_dec >= start_sn && sn_dec <= start_sn + num_of_pSDAQ-1)
				{
					if(strstr(argv[2],"addr"))
					{
						if(argv[3])
						{
							if(strstr(argv[3],"park"))
							{
								pthread_mutex_lock(&SDAQs_mem_access[sn_dec - start_sn]);
									pSDAQs_mem[sn_dec-start_sn].address = Parking_address;
									pSDAQs_mem[sn_dec-start_sn].status &= ~(0x01); // stop measure in address change
								pthread_mutex_unlock(&SDAQs_mem_access[sn_dec - start_sn]);
								return;
							}
							else
							{
								unsigned char addr_dec = atoi(argv[3]);//channel_dec of pseudoSDAQ
								if(addr_dec && addr_dec < Parking_address)
								{
									pthread_mutex_lock(&SDAQs_mem_access[sn_dec - start_sn]);
										pSDAQs_mem[sn_dec-start_sn].address = addr_dec;
									pthread_mutex_unlock(&SDAQs_mem_access[sn_dec - start_sn]);
									return;
								}
							}
						}
					}
					else if((channel_str = strstr(argv[2],"ch")))
					{
						channel_dec = atoi(channel_str+2);//channel number
						if(channel_dec >= 1 && channel_dec <= pSDAQs_mem[sn_dec - start_sn].number_of_channels && argv[3])
						{
							pthread_mutex_lock(&SDAQs_mem_access[sn_dec - start_sn]);
								if(!strcmp(argv[3],"date"))
								{
									if(argv[4])//expiration date
									{
										if(!strcmp(argv[4],"now"))//if argument is "now"
											pSDAQs_mem[sn_dec-start_sn].ch_cal_date[channel_dec-1].date = time(NULL);
										else if(strcmp(argv[4],"-"))//if argument is not "-"
										{
											struct tm exp_date_dec;
											if(!exp_date_dec_validator(&exp_date_dec,argv[4]))
												pSDAQs_mem[sn_dec-start_sn].ch_cal_date[channel_dec-1].date = mktime(&exp_date_dec);
											else
												printw("\n Argument of Date is invalid");
										}
									}
									if(argv[5])//amount of points
									{
										if(strcmp(argv[5],"-"))
										{
											sprintf(str_buff,"%i",atoi(argv[5]));
											if(strstr(str_buff,argv[5]) && atoi(argv[5])>= 0 && atoi(argv[5])<=16)
												pSDAQs_mem[sn_dec-start_sn].ch_cal_date[channel_dec-1].amount_of_points = atoi(argv[5]);
											else
												printw("\n Argument for amount of points is invalid");
										}
									}
									if(argv[6])//Unit code
									{
										sprintf(str_buff,"%i",atoi(argv[6]));
										if(strstr(str_buff,argv[6]))
											pSDAQs_mem[sn_dec-start_sn].ch_cal_date[channel_dec-1].cal_units = atoi(argv[6]);
										else
											printw("\n Argument of units is not a number");
									}
								}
								else if(!strcmp(argv[3],"noise"))
									pSDAQs_mem[sn_dec-start_sn].noise |= 1<<(channel_dec-1);
								else if(!strcmp(argv[3],"nonoise"))
									pSDAQs_mem[sn_dec-start_sn].noise &= ~(1<<(channel_dec-1));
								else if(!strcmp(argv[3],"sensor"))
									pSDAQs_mem[sn_dec-start_sn].nosensor &= ~(1<<(channel_dec-1));
								else if(!strcmp(argv[3],"nosensor"))
									pSDAQs_mem[sn_dec-start_sn].nosensor |= 1<<(channel_dec-1);
								else
								{	//check if the argument is number
									sprintf(str_buff,"%f",atof(argv[3]));
									if(strstr(str_buff,argv[3]))
									{
										pSDAQs_mem[sn_dec-start_sn].out_val[channel_dec-1] = atof(argv[3]);
										pSDAQs_mem[sn_dec-start_sn].nosensor &= ~(1<<(channel_dec-1));
									}
									else
										printw("\nError: out_value argument is not a number");
								}
							pthread_mutex_unlock(&SDAQs_mem_access[sn_dec - start_sn]);
							return;
						}
					}
					else if(!strcmp(argv[2],"all"))
					{
						pthread_mutex_lock(&SDAQs_mem_access[sn_dec - start_sn]);
							if(!strcmp(argv[3],"noise"))
								pSDAQs_mem[sn_dec-start_sn].noise = -1;
							else if(!strcmp(argv[3],"nonoise"))
								pSDAQs_mem[sn_dec-start_sn].noise = 0;
							else if(!strcmp(argv[3],"nosensor"))
								pSDAQs_mem[sn_dec-start_sn].nosensor = -1;
							else
							{	//check if the argument is number
								sprintf(str_buff,"%f",atof(argv[3]));
								if(strstr(str_buff,argv[3]))
								{
									pSDAQs_mem[sn_dec-start_sn].nosensor = 0;
									for(int i=0;i<pSDAQs_mem[sn_dec-start_sn].number_of_channels;i++)
										pSDAQs_mem[sn_dec-start_sn].out_val[i] = atof(argv[3]);
								}
								else
									printw("\nError: out_value argument is not a number");
							}
						pthread_mutex_unlock(&SDAQs_mem_access[sn_dec - start_sn]);
						return;
					}
					else if(!strcmp(argv[2],"amount"))// amount of channels
					{
						if(argv[3])
						{
							if(atoi(argv[3])>0 && atoi(argv[3])<=16)
							{
								pthread_mutex_lock(&SDAQs_mem_access[sn_dec - start_sn]);
									pSDAQs_mem[sn_dec-start_sn].number_of_channels = atoi(argv[3]);
								pthread_mutex_unlock(&SDAQs_mem_access[sn_dec - start_sn]);
								return;
							}
						}
					}
				}
			}
		}
	}
	printw("\n  ????");
}
//SDAQ_psim shell help
void shell_help()
{
	const int height = 28;
	const int width = 90;
	int starty = (LINES - height) / 2;	/* Calculating for a center placement */
	int startx = (COLS - width) / 2;	/* of the window		*/
	WINDOW *help_win = newwin(height, width, starty, startx);
	keypad(help_win, TRUE);
	curs_set(0);//hide cursor
	//scrollok(help_win, TRUE);
	do{
		mvwprintw(help_win,1,1,"%s",shell_help_str);
		wprintw(help_win,"\n\n\n  Press Ctrl+C to exit help");
		box(help_win, 0 , 0);
		wrefresh(help_win);
	}while(getch()!=3);
	wborder(help_win, ' ', ' ', ' ',' ',' ',' ',' ',' ');
	wclear(help_win);
	wrefresh(help_win);
	delwin(help_win);
	curs_set(1);//hide cursor
}

