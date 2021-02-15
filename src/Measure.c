/*
File: Measure.c, implementation of the measure mode, part of the SDAQ_worker.
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

/* ncurses windows sizes definitions*/
#define w_stat_info_height 9
#define w_stat_info_width 30
#define w_meas_height 20
#define w_spacing 0
#define w_meas_width  w_stat_info_width
#define term_min_width  w_meas_width*2 + w_spacing
#define term_min_height  w_meas_height + w_stat_info_height + 4

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <ncurses.h>
#include <signal.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include "SDAQ_drv.h"
#include "Modes.h"

struct thread_arguments_passer
{
	unsigned char lock_kb_flag;
	int socket_num;
	unsigned char dev_addr;
	char *CANif_name;
	WINDOW *meas_win,*status_win,*info_win,*raw_meas_win;
};

//global variables
volatile char running=1,box_flag=0,raw_flag=0; //Flag to activate RAW_measurement message from the device
pthread_mutex_t display_access = PTHREAD_MUTEX_INITIALIZER;

//local functions
short time_diff_cal(unsigned short dev_time, unsigned short ref_time);//assistance func for timestamp diff
void w_init(struct thread_arguments_passer *arg);//init apps ncurses windows
void wclean_refresh(WINDOW *ptr);//clean a window and redraw it.
void *CAN_socket_RX(void *varg_pt);//Thread function
const char * status_byte_dec(unsigned char status_byte,unsigned char field);

int Measure(int socket_num, unsigned char dev_addr, opt_flags *usr_flag)
{
	//Variables for ncurses
	int row,col,last_row=0,last_col=0;
	char user_pressed_key;
	struct winsize term_init_size;
	//variables for threads
	pthread_t CAN_socket_RX_Thread_id;
	struct thread_arguments_passer thread_arg;

	thread_arg.dev_addr = dev_addr;
	thread_arg.socket_num = socket_num;
	thread_arg.CANif_name = usr_flag->CANif_name;
	thread_arg.lock_kb_flag = 0;
	if(usr_flag->resize)
		printf("\e[8;%d;%dt",term_min_height,term_min_width);//resize terminal window to the application's needs
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &term_init_size);// get current size of terminal window
	//Check if the terminal have the minimum size for the application
	if(term_init_size.ws_col<term_min_width || term_init_size.ws_row<term_min_height)
	{
		printf("Terminal need to be at least %dX%d Characters\n",term_min_width,term_min_height);
		return EXIT_SUCCESS;
	}
	//Init Measurement mode with ncurses
	initscr(); // start the ncurses mode
	raw();//getch without return
	noecho();//disable echo
	curs_set(0);//hide cursor
	scrollok(stdscr, TRUE);
	w_init(&thread_arg);
	//mount the CAN-bus receiver on a thread, and load arguments
	pthread_create(&CAN_socket_RX_Thread_id, NULL, CAN_socket_RX, &thread_arg);
	while(running>0)
	{
		getmaxyx(stdscr,row,col);
		if(row>=term_min_height && col>=term_min_width)
		{
			if(last_row!=row||last_col!=col)//reset display in cases of terminal resize, clear request and on first run
			{
				pthread_mutex_lock(&display_access);
					w_init(&thread_arg);
				pthread_mutex_unlock(&display_access);
				QueryDeviceInfo(socket_num,dev_addr);
				last_row = row;
				last_col = col;
			}
			user_pressed_key=getch();// get the user's entrance
			if(!thread_arg.lock_kb_flag)
			{
				switch(user_pressed_key)
				{
					case '1': Req_Raw_meas(socket_num,dev_addr,raw_flag); Start(socket_num,dev_addr); break;
					case '2': Req_Raw_meas(socket_num,dev_addr,raw_flag); Stop(socket_num,dev_addr); last_row=last_col=0; break;
					case 'Q':
					case 'q':
					case  3 : running=0; break; //SIGINT or Ctrl+C
					case 'R': raw_flag^=1; Req_Raw_meas(socket_num,dev_addr,raw_flag); break;
					case 'C':
					case 'B': box_flag^=1;//toggle borders and force clean
					case '3': QueryDeviceInfo(socket_num,dev_addr); last_row=last_col=0; break;
					case 'L': thread_arg.lock_kb_flag = 1; last_row=last_col=0; break;
				}
			}
			else // enter if keyboard is locked
			{
				running = user_pressed_key==3 ? 0 : 1; //quit on Ctrl+C
				thread_arg.lock_kb_flag = user_pressed_key=='L' ? 0 : 1; //unlock keyboard
				last_row=last_col=0;
			}
			if(!raw_flag) //clean Raw_meas window if the flag is off and the measurements is off
			{
				pthread_mutex_lock(&display_access);
					refresh();
					wclean_refresh(thread_arg.raw_meas_win);
				pthread_mutex_unlock(&display_access);
			}
		}
		else
			running = -1;
	}
	pthread_cancel(CAN_socket_RX_Thread_id);// stop "CAN_socket_RX_Thread_id" thread
	pthread_detach(CAN_socket_RX_Thread_id);
	endwin();
	if(usr_flag->resize)
		printf("\e[8;%d;%dt",term_init_size.ws_row,term_init_size.ws_col);//restore the terminal size
	if(running<0)
		printf("Terminal need to be at least %dx%d\n",term_min_width,term_min_height);
	return EXIT_SUCCESS;
}

void wclean_refresh(WINDOW *ptr)
{
	wclear(ptr);
	if(box_flag)
		box(ptr,0,0);
	wrefresh(ptr);
	return;
}

void w_init(struct thread_arguments_passer *arg)
{
	int term_col,term_row;
	getmaxyx(stdscr,term_row,term_col);
	arg->status_win   = newwin(w_stat_info_height,w_stat_info_width, 1, term_col/2-w_stat_info_width-w_spacing/2);
	arg->info_win     = newwin(w_stat_info_height,w_stat_info_width, 1, term_col/2+w_spacing/2);
	arg->meas_win     = newwin(w_meas_height,w_meas_width, 1+w_stat_info_height, term_col/2-w_meas_width-w_spacing/2);
	arg->raw_meas_win = newwin(w_meas_height,w_meas_width, 1+w_stat_info_height, term_col/2+w_spacing/2);
	scrollok(arg->status_win, TRUE);
	scrollok(arg->info_win, TRUE);
	scrollok(arg->meas_win, TRUE);
	scrollok(arg->raw_meas_win, TRUE);
	mvprintw(0,0,"%d %d",term_row,term_col);//ncurses stdscr size -- does not show in the screen, move after clean
	clear();
	mvprintw(0,term_col/2-14,"Device Address: %d (%s)", arg->dev_addr, arg->CANif_name);
	mvprintw(term_min_height-2,term_col/2-w_stat_info_width,"Function Buttons:");
	if(arg->lock_kb_flag)
		printw(" Locked");
	mvprintw(term_min_height-1,term_col/2-w_stat_info_width,"Q Exit 1 Start 2 Stop 3 Info_Req R Raw_meas L (Un)Lock");
	refresh();
	wclean_refresh(arg->status_win);
	wclean_refresh(arg->info_win);
	wclean_refresh(arg->meas_win);
	wclean_refresh(arg->raw_meas_win);
	return;
}

//Thread function. Act as CAN-bus message Receiver and decoder for SDAQ devices
void * CAN_socket_RX(void *varg_pt)
{
	//term size
	int term_col;
	//passed arguments decoder
	struct thread_arguments_passer *arg = (struct thread_arguments_passer *) varg_pt;
	//local variables for CAN Socket frame and SDAQ messages decoders
	unsigned char dev_type = 0;
	char timediff_str[20];
	struct can_frame frame_rx;
	int RX_bytes;
	sdaq_can_id *id_dec = (sdaq_can_id *)&(frame_rx.can_id);
	sdaq_status *status_dec = (sdaq_status *)frame_rx.data;
	sdaq_meas *meas_dec = (sdaq_meas *)frame_rx.data;
	sdaq_info *info_dec = (sdaq_info *)frame_rx.data;
	sdaq_sysvar *sysvar_dec = (sdaq_sysvar *)frame_rx.data;
	sdaq_sync_debug_data *ts_dec = (sdaq_sync_debug_data *)frame_rx.data;
	while(running)
	{
		RX_bytes=read(arg->socket_num, &frame_rx, sizeof(frame_rx));
		if(RX_bytes==sizeof(frame_rx))
		{
			if(arg->dev_addr==id_dec->device_addr)
			{
				pthread_mutex_lock(&display_access);
					switch(id_dec->payload_type)
					{
						case Uncalibrated_meas:
							raw_flag=1;
							mvwprintw(arg->raw_meas_win,1,2,"Un-calibrated(Raw):");
							mvwprintw(arg->raw_meas_win,2,4,"Time -> %5d (msec)",meas_dec->timestamp);
							if(!(meas_dec->status))
								mvwprintw(arg->raw_meas_win,id_dec->channel_num-1+3,4,"CH%02d = %9.3f %-4s"
													,id_dec->channel_num,meas_dec->meas,unit_str[meas_dec->unit]);
							else
								mvwprintw(arg->raw_meas_win,id_dec->channel_num-1+3,4,"CH%02d =    No sensor    ",id_dec->channel_num);
							wrefresh(arg->raw_meas_win);
							break;
						case Measurement_value:
							mvwprintw(arg->meas_win,1,2,"Calibrated:");
							mvwprintw(arg->meas_win,2,4,"Time -> %5d (msec)",meas_dec->timestamp);
							if(!(meas_dec->status))
								mvwprintw(arg->meas_win,id_dec->channel_num-1+3,4,"CH%02d = %9.3f %s%3s  "
													,id_dec->channel_num,meas_dec->meas,unit_str[meas_dec->unit]
													,meas_dec->unit<Unit_code_base_region_size?"(B)":"");
							else if(meas_dec->status&(1<<No_sensor))
								mvwprintw(arg->meas_win,id_dec->channel_num-1+3,4,"CH%02d =    No sensor    ",id_dec->channel_num);
							else if(meas_dec->status&(1<<Out_of_range))
								mvwprintw(arg->meas_win,id_dec->channel_num-1+3,4,"CH%02d =    Out of range     ",id_dec->channel_num);
							else if(meas_dec->status&(1<<Over_range))
								mvwprintw(arg->meas_win,id_dec->channel_num-1+3,4,"CH%02d =    Over Range       ",id_dec->channel_num);
							wrefresh(arg->meas_win);
							break;
						case Device_status:
							mvwprintw(arg->status_win,1,1,"Device_status & S/N:");
							mvwprintw(arg->status_win,2,3,"S/N = %d",status_dec->dev_sn);
							mvwprintw(arg->status_win,3,3,"Mode  : %3s ",status_byte_dec(status_dec->status,Mode));
							mvwprintw(arg->status_win,4,3,"State : %9s",status_byte_dec(status_dec->status,State));
							mvwprintw(arg->status_win,5,3,"Error?  : %3s",status_byte_dec(status_dec->status,Error));
							mvwprintw(arg->status_win,6,3,"IsSync? : %3s",status_byte_dec(status_dec->status,In_sync));
							wrefresh(arg->status_win);
							if(!(status_dec->status & 1<<State))//no measure
							{
								wclean_refresh(arg->meas_win);
								wclean_refresh(arg->raw_meas_win);
							}
							//clear "Error: Socket Timeout" print
							move(term_min_height-3,0);
							clrtoeol();
							refresh();
							break;
						case Device_info:
							dev_type = info_dec->dev_type;
							mvwprintw(arg->info_win,1,1,"Device_info:");
							mvwprintw(arg->info_win,2,3,"Type = %s",dev_type_str[dev_type]);
							mvwprintw(arg->info_win,3,3,"Firmware rev = %d",info_dec->firm_rev);
							mvwprintw(arg->info_win,4,3,"Hardware rev = %d",info_dec->hw_rev);
							mvwprintw(arg->info_win,5,3,"Channels = %-2d",info_dec->num_of_ch);
							mvwprintw(arg->info_win,6,3,"Samplerate = %d",info_dec->sample_rate);
							mvwprintw(arg->info_win,7,3,"Max Cal points = %d",info_dec->max_cal_point);
							wrefresh(arg->info_win);
							if(*dev_input_mode_str[dev_type])//Check if device have available input mode.
								QuerySystemVariables(arg->socket_num, arg->dev_addr);
							break;
						case System_variable:
							if(*dev_input_mode_str[dev_type])
							{
								if(!sysvar_dec->type && sysvar_dec->var_val.as_uint32<INP_MODE_MAX_COL)
								{
									mvwprintw(arg->info_win,2,3,"Type = %s/%s", dev_type_str[dev_type],
																			    dev_input_mode_str[dev_type][sysvar_dec->var_val.as_uint32]);
									wrefresh(arg->info_win);
								}
							}
							break;
						case Sync_Info:
							sprintf(timediff_str, "%hu msec",time_diff_cal(ts_dec->dev_time,ts_dec->ref_time));
							mvwprintw(arg->status_win,7,3,"Timediff : %-11s",timediff_str);
							wrefresh(arg->status_win);
						default:
							break;
					}
				pthread_mutex_unlock(&display_access);
			}
		}
		else
		{
			pthread_mutex_lock(&display_access);
				move(term_min_height-3, 0);
				clrtoeol();
				term_col = getmaxx(stdscr);
				mvprintw(term_min_height-3,term_col/2-10,"Error: Socket Timeout");
				refresh();
			pthread_mutex_unlock(&display_access);
		}
	}
	return NULL;
}
short time_diff_cal(unsigned short dev_time, unsigned short ref_time)
{
	short ret = dev_time > ref_time ? dev_time - ref_time : ref_time - dev_time;
	if(ret<0)
		ret = 60000 - dev_time - ref_time;
	return ret;
}
