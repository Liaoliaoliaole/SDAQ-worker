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
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include "SDAQ_drv.h"
#include "Modes.h"

struct thread_arguments_passer
{
	int socket_num;
	unsigned char dev_addr;
	WINDOW *meas_win,*status_win,*info_win,*raw_meas_win;
};

//Terminal size and ncurses constants
const int w_stat_info_height = 8;
const int w_stat_info_width = 30;
const int w_meas_height = 19;
const int w_meas_width = w_stat_info_width;
const int w_spacing = 0;
const int term_min_width = w_meas_width*2 + w_spacing;
const int term_min_height = w_meas_height + w_stat_info_height + 4;

//global variables
volatile char running=1,box_flag=0,raw_flag=0; //Flag to activate RAW_measurement message from the device
pthread_mutex_t display_access = PTHREAD_MUTEX_INITIALIZER;

//local functions
void w_init(struct thread_arguments_passer *arg);
void wclean_refresh(WINDOW *ptr);
void *CAN_socket_RX(void *varg_pt);//Thread function 
const char * status_byte_dec(unsigned char status_byte,unsigned char field);

int Measure(int socket_num, unsigned char dev_addr, opt_flags usr_flag)
{
	//Variables for ncurses
	int row,col,last_row=0,last_col=0;
	char user_pressed_key;
	struct winsize term_init_size;
	//variables for threads
	pthread_t CAN_socket_RX_Thread_id; 
	struct thread_arguments_passer thread_arg;
	
	thread_arg.dev_addr=dev_addr;
	thread_arg.socket_num=socket_num;
	
	//Init Measurement mode with ncurses
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &term_init_size);// get current size of terminal window 
	//Check if the terminal have the minimum size for the application
	if(term_init_size.ws_col<term_min_width && term_init_size.ws_row<term_min_height)
	{
		printf("Terminal need to be at least %dX%d Characters\n",term_min_width,term_min_height);
		return EXIT_SUCCESS;
	}
	printf("\e[8;%d;%dt",term_min_height,term_min_width);//resize terminal window to the application's needs
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
			switch(user_pressed_key)
			{
				case '1': Raw_meas(socket_num,dev_addr,raw_flag); Start(socket_num,dev_addr); break;
				case '2': Raw_meas(socket_num,dev_addr,raw_flag); Stop(socket_num,dev_addr); last_row=last_col=0; break;
				case 'S': Sync(socket_num,0);
				case '3': QueryDeviceInfo(socket_num,dev_addr); break;
				case 'Q':
				case 'q': 
				case  3 : running=0; break; //SIGINT or Ctrl+c
				case 'R': raw_flag^=0x01; Raw_meas(socket_num,dev_addr,raw_flag); break;
				case 'B': box_flag^=1;//toggle borders and force clean
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
		else
			running = -1;
	}
	pthread_cancel(CAN_socket_RX_Thread_id);// cancel "CAN_socket_RX_Thread_id" thread
	endwin();
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
	arg->status_win  = newwin(w_stat_info_height,w_stat_info_width, 1, term_col/2-w_stat_info_width-w_spacing/2);
	arg->info_win    = newwin(w_stat_info_height,w_stat_info_width, 1, term_col/2+w_spacing/2);
	arg->meas_win    = newwin(w_meas_height,w_meas_width, 1+w_stat_info_height, term_col/2-w_meas_width-w_spacing/2);
	arg->raw_meas_win= newwin(w_meas_height,w_meas_width, 1+w_stat_info_height, term_col/2+w_spacing/2);
	scrollok(arg->status_win, TRUE);
	scrollok(arg->info_win, TRUE);
	scrollok(arg->meas_win, TRUE);
	scrollok(arg->raw_meas_win, TRUE);
	mvprintw(0,0,"%d %d",term_row,term_col);//ncurses stdscr size -- does not show in the screen, move after clean 
	clear();
	mvprintw(0,term_col/2-8,"Device Address: %d",arg->dev_addr);
	mvprintw(term_min_height-2,term_col/2-w_stat_info_width,"Function Buttons:");
	mvprintw(term_min_height-1,term_col/2-w_stat_info_width,"Q Exit 1 Start 2 Stop 3 Dev_Info R Un-Calibrated S Sync");
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
	/*
	unsigned char amount_of_inputs=1,avg_cnt,i; //Averaging counter, scanning index and amound of
	float meas_value[16]={0.0};
	*/
	//term size 
	int term_col,term_row;
	//passed arguments decoder
	struct thread_arguments_passer *arg = (struct thread_arguments_passer *) varg_pt;
	//local variables for CAN Socket frame and SDAQ messages decoders
	struct can_frame frame_rx;
	int RX_bytes;
	sdaq_can_id *id_dec;
	sdaq_status *status_dec;
	sdaq_meas *meas_dec;
	sdaq_info *info_dec;
	
	while(1)
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
								mvwprintw(arg->raw_meas_win,id_dec->channel_num-1+2,4,"CH%02d = %04.3f %s   "
													,id_dec->channel_num,meas_dec->meas,unit_str[meas_dec->unit]);
							else
								mvwprintw(arg->raw_meas_win,id_dec->channel_num-1+2,4,"CH%02d = No sensor  ",id_dec->channel_num);
							wrefresh(arg->raw_meas_win);
							break;
						case Measurement_value: 
							//wclear(arg->meas_win);
							meas_dec = (sdaq_meas *)frame_rx.data;
							mvwprintw(arg->meas_win,1,2,"Calibrated:");
							if(!(meas_dec->status))
								mvwprintw(arg->meas_win,id_dec->channel_num-1+2,4,"CH%02d = %04.3f %s   "
													,id_dec->channel_num,meas_dec->meas,unit_str[meas_dec->unit]);
							else
								mvwprintw(arg->meas_win,id_dec->channel_num-1+2,4,"CH%02d = No sensor  ",id_dec->channel_num);
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
							mvwprintw(arg->status_win,2,3,"S/N = %d",status_dec->dev_sn);
							mvwprintw(arg->status_win,3,3,"State : %9s",status_byte_dec(status_dec->status,State));								
							mvwprintw(arg->status_win,4,3,"IsSync? : %3s",status_byte_dec(status_dec->status,In_sync));
							mvwprintw(arg->status_win,5,3,"Error?  : %3s",status_byte_dec(status_dec->status,Error));
							mvwprintw(arg->status_win,6,3,"Mode  : %3s",status_byte_dec(status_dec->status,Mode));
							wrefresh(arg->status_win);
							if(!(status_dec->status & 0x01))
								wclean_refresh(arg->meas_win);
							break;
						case Device_info: 
							//wclear(arg->meas_win);
							info_dec = (sdaq_info *)frame_rx.data;
							mvwprintw(arg->info_win,1,1,"Device_info:");
							mvwprintw(arg->info_win,2,3,"Type = %s",dev_type_str[info_dec->dev_type]);
							mvwprintw(arg->info_win,3,3,"Firmware rev = %d",info_dec->firm_rev);
							mvwprintw(arg->info_win,4,3,"Hardware rev = %d",info_dec->hw_rev);
							mvwprintw(arg->info_win,5,3,"Channels = %d",info_dec->num_of_ch); 
							mvwprintw(arg->info_win,6,3,"Samplerate = %d",info_dec->sample_rate);
							wrefresh(arg->info_win);
							//amount_of_inputs=info_dec->num_of_ch; //used in averaging as end index 
							break;
						default: 
							break; 
					}
				pthread_mutex_unlock(&display_access);
			}
		}
		else
		{
			pthread_mutex_lock(&display_access);
				getmaxyx(stdscr,term_row,term_col);
				mvprintw(term_row-3,term_col/2-10,"Error: Socket Timeout");
				refresh();
			pthread_mutex_unlock(&display_access);
		}
	}
	return NULL;
}
