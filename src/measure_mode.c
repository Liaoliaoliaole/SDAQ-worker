#define w_xoffset 
#define w_yoffset 
  
#define AVG_INTERVAL 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <ncurses.h> 
#include <signal.h>
#include <pthread.h> 

#include <linux/can.h>
#include <linux/can/raw.h>

#include "sdaq_drv.h"
#include "modes.h"

struct thread_arguments_passer
{
	int socket_num;
	unsigned char dev_addr;
	WINDOW *meas_win,*status_win,*info_win,*raw_meas_win;
};

//global variables
volatile char running=1,box_flag=0,raw_flag=0; //Flag to activate RAW_measurement message from the device
pthread_mutex_t display_access = PTHREAD_MUTEX_INITIALIZER;


//local functions
void wclean_refresh(WINDOW *ptr);

//threaded function. Act as CAN-bus message Receiver and decoder for SDAQ devices
void * CAN_socket_RX(void *varg_pt) 
{ 
	/*
	unsigned char amount_of_inputs=1,avg_cnt,i; //Averaging counter, scanning index and amound of
	float meas_value[16]={0.0};
	*/
	//passed arguments decoder
	struct thread_arguments_passer *arg = (struct thread_arguments_passer *) varg_pt; 
	//local variables for CAN Socket frame and SDAQ messages decoders
	struct can_frame frame_rx;
	int RX_bytes;
	sdaq_can_id *id_dec;
	sdaq_status *status_dec;
	sdaq_meas *meas_dec;
	sdaq_info *info_dec;
	
	while(running>0)
	{		
		RX_bytes=read(arg->socket_num, &frame_rx, sizeof(frame_rx));
		if(RX_bytes==sizeof(frame_rx))
		{
			id_dec = (sdaq_can_id *)&(frame_rx.can_id);
			if(arg->dev_addr==id_dec->device_addr)
			{
				pthread_mutex_lock(&display_access);
					switch(id_dec->payload_type)
					{
						case Uncalibrated_meas:
							raw_flag=1;
							//wclear(arg->raw_meas_win);
							meas_dec = (sdaq_meas *)frame_rx.data;
							mvwprintw(arg->raw_meas_win,1,2,"Uncalibrated:");
							if(!(meas_dec->status))
								mvwprintw(arg->raw_meas_win,id_dec->channel_num-1+2,4,"CH%2d = %04.3f %s   "
													,id_dec->channel_num,meas_dec->meas,unit_str[meas_dec->unit]);
							else
								mvwprintw(arg->raw_meas_win,id_dec->channel_num-1+2,4,"CH%2d = No sensor  ",id_dec->channel_num);
							wrefresh(arg->raw_meas_win);
							break;
						case Measurement_value: 
							//wclear(arg->meas_win);
							meas_dec = (sdaq_meas *)frame_rx.data;
							mvwprintw(arg->meas_win,1,2,"Calibrated:");
							if(!(meas_dec->status))
								mvwprintw(arg->meas_win,id_dec->channel_num-1+2,4,"CH%2d = %04.3f %s   "
													,id_dec->channel_num,meas_dec->meas,unit_str[meas_dec->unit]);
							else
								mvwprintw(arg->meas_win,id_dec->channel_num-1+2,4,"CH%2d = No sensor  ",id_dec->channel_num);
							wrefresh(arg->meas_win);
							/*
							if(!(meas_dec->status))
							{
								if(isnan(meas_value[(id_dec->channel_num)-1]))//
									meas_value[(id_dec->channel_num)-1]=0.0;
								meas_value[(id_dec->channel_num)-1]+=meas_dec->meas;
							}
							else
								meas_value[(id_dec->channel_num)-1]=NAN;
							avg_cnt++;
							if (avg_cnt>=AVG_INTERVAL*amount_of_inputs)
							{
								for(i=0;i<amount_of_inputs;i++)
								{
									if(!(isnan(meas_value[i])))
									{
										mvwprintw(arg->meas_win,i+2,4,"CH%2d = %04.3f%s "
															,i+1,meas_value[i]/AVG_INTERVAL,unit_str[meas_dec->unit]);
										meas_value[i]=0.0;
									}
									else
										mvwprintw(arg->meas_win,i+2,4,"CH%2d = No sensor ",i+1);
								}
								wrefresh(arg->meas_win);
								avg_cnt=0;
							}*/
							break;
						case Device_status: 
							//wclear(arg->status_win);
							status_dec = (sdaq_status *)frame_rx.data;
							mvwprintw(arg->status_win,1,1,"Device_status & S/N:"); 
							mvwprintw(arg->status_win,2,2,"Dev serial number = %d",status_dec->dev_sn);
							mvwprintw(arg->status_win,3,2,"Dev status = %d",status_dec->status);
							mvwprintw(arg->status_win,4,2,"Dev type = %s (%d)",dev_type_str[status_dec->device_type],
																			  status_dec->device_type);
							wrefresh(arg->status_win);
							if(!(status_dec->status & 0x01))
								wclean_refresh(arg->meas_win);
							break;
						case Device_info: 
							//wclear(arg->meas_win);
							info_dec = (sdaq_info *)frame_rx.data;
							mvwprintw(arg->info_win,1,2,"Device_info:");
							mvwprintw(arg->info_win,2,3,"Device type = %s (%d)",dev_type_str[info_dec->dev_type],info_dec->dev_type);
							mvwprintw(arg->info_win,3,3,"Firmware rev = %d",info_dec->firm_rev);
							mvwprintw(arg->info_win,4,3,"Hardware rev = %d",info_dec->hw_rev);
							mvwprintw(arg->info_win,5,3,"Number of channels = %d",info_dec->num_of_ch);
							mvwprintw(arg->info_win,6,3,"Samplerate = %d sps",info_dec->sample_rate);
							wrefresh(arg->info_win);
							//amount_of_inputs=info_dec->num_of_ch; //used in averaging as end index 
							break;
						/*
						case Calibration_Date: 
							//wclear(arg->meas_win); 
							mvwprintw(arg->meas_win,1,1,"Calibration_Date"); 
							wrefresh(arg->meas_win);
							break;
						*/
						default: break; 
					}
				pthread_mutex_unlock(&display_access);
			}
		}
		else
		{
			mvprintw(28,25,"Error: Socket Timeout");
			refresh();
		}
	}
	return NULL;
}

void Measure_SDAQ(int socket_num,unsigned char dev_addr)
{
	//Variables for ncurses
	int row,col,last_row=0,last_col=0;
	char user_pressed_key;
	//variables for threads
	pthread_t CAN_socket_RX_Thread_id; 
	struct thread_arguments_passer thread_arg;
	
	thread_arg.dev_addr=dev_addr;
	thread_arg.socket_num=socket_num;
	//Stop any measurements on CAN-bus
	//Stop(socket_num, 0);
	
	//Init Measurement mode with ncurses
	system("printf '\e[8;31;71t'");
	initscr(); // start the curses mode
	raw();//getch without return
	noecho();//disable echo
	cbreak();//exit on break
	curs_set(0);//hide cursor
	thread_arg.status_win  = newwin(8,35, 1, 0);//create window for status
	thread_arg.info_win    = newwin(8,35, 1, 36);//create window for info
	thread_arg.meas_win    = newwin(19,35, 9, 0);//create window for measurements
	thread_arg.raw_meas_win= newwin(19,35, 9, 36);//create window for measurements
	
	//mount the CAN-bus receiver on a thread, and load arguments 
	pthread_create(&CAN_socket_RX_Thread_id, NULL, CAN_socket_RX, &thread_arg);
	sleep(1);
	//QueryDeviceInfo(socket_num,dev_addr);
	while(running>0)
	{
		getmaxyx(stdscr,row,col);
		if(last_row!=row||last_col!=col)//reset display in cases of terminal resize, clear request and on first run
		{
			if(col<50&&row<50)//check if the terminal is smaller that the requirement 
				running = -1;
			else
			{
				pthread_mutex_lock(&display_access);
					clear();
					mvprintw(0,25,"Device Address:%d",dev_addr);
					mvprintw(row-2,0,"Function Buttons:\n");
					printw("'q' Quit 1 Start 2 Stop 3 Dev_Info R Un-Calibrated ");
					refresh();
					wclean_refresh(thread_arg.status_win);
					wclean_refresh(thread_arg.info_win);
					wclean_refresh(thread_arg.meas_win);
					wclean_refresh(thread_arg.raw_meas_win);
				pthread_mutex_unlock(&display_access);
				QueryDeviceInfo(socket_num,dev_addr);
				last_col=col;
				last_row=row;
			}
		}
		user_pressed_key=getch();// get the user's entrance 
		switch(user_pressed_key)
		{
			case '1': raw_flag=0; Raw_meas(socket_num,dev_addr,raw_flag); Start(socket_num,dev_addr); break;
			case '2': raw_flag=0; Raw_meas(socket_num,dev_addr,raw_flag); Stop(socket_num,dev_addr);  break;
			case '3': QueryDeviceInfo(socket_num,dev_addr);break;
			case 'q': running=0; break;
			case 'R': raw_flag^=0x01; Raw_meas(socket_num,dev_addr,raw_flag); break;
			case 'B': last_row=last_col=0;box_flag^=1;//toggle borders and force clean
			case 'C': last_row=last_col=0;
			default : break;
		}
		if(!raw_flag) //clean Raw_meas window if the flag is off and the measurements is off
		{
			pthread_mutex_lock(&display_access);
				refresh();
				wclean_refresh(thread_arg.raw_meas_win);
			pthread_mutex_unlock(&display_access);
		}
	}
	endwin();
	if(running<0)
		printf("Terminal need to be at least %dx%d\n",71,31);
}

void wclean_refresh(WINDOW *ptr)
{
	wclear(ptr);
	if(box_flag)
		box(ptr,0,0);
	wrefresh(ptr);
	return;
}