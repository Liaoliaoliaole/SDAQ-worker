/*
File: SDAQ_psim_UI. The user interface for the SDAQ_psim.
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

#define user_inp_buf_size 80
#define max_amount_of_user_arg 20
#define history_buff_length 10

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <ncurses.h>
#include <glib.h>
#include <gmodule.h>

//Include Application's header
#include "SDAQ_psim_types.h"

typedef struct{
	char usr_in_buff[user_inp_buf_size];
}history_buffer_entry;

//Global Shared Variables
pthread_mutex_t *SDAQs_mem_access;
unsigned char SDAQ_psim_run=1;

//function for decode user input
int user_inp_dec(char **argv, char *usr_in_buff, unsigned int start_sn, unsigned char num_of_pSDAQ, pSDAQ_memory_space *pSDAQs_mem);
//function for execution of user's command input
void user_com(unsigned int argc, char **argv, unsigned int start_sn, unsigned char num_of_pSDAQ, pSDAQ_memory_space *pSDAQs_mem);
//SDAQ_psim shell help
void shell_help();

void print_hist_buffs(gpointer data,gpointer user_data)
{
	static int i = 0;
	history_buffer_entry *node_data = data;
	printf("buffer %d -> %s\n",i++,node_data->usr_in_buff);
}

//slice free function for history_buffs_nodes
void history_buff_free_node(gpointer node)
{
	g_slice_free(history_buffer_entry, node);
}

//Implementation of the user's Interface function
void user_interface(unsigned int start_sn, unsigned char num_of_pSDAQ, pSDAQ_memory_space *pSDAQs_mem)
{
	unsigned int end_index=0, cur_pos=0, key, argc, last_curx, history_buffs_index=0;
	char *argv[max_amount_of_user_arg] = {NULL};
	GQueue hist_buffs = G_QUEUE_INIT;
	g_queue_push_head(&hist_buffs, g_slice_alloc0(sizeof(history_buffer_entry)));
	gpointer nth_node = NULL;
	char *usr_in_buff = ((history_buffer_entry *)g_queue_peek_head(&hist_buffs))->usr_in_buff;

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
				if((nth_node = g_queue_peek_nth(&hist_buffs,history_buffs_index+1)))
				{
					usr_in_buff = ((history_buffer_entry *)nth_node)->usr_in_buff;
					history_buffs_index++;
					move(getcury(stdscr),3);
					clrtoeol();
					printw("%s",usr_in_buff);
					end_index = strlen(usr_in_buff);
					cur_pos = end_index;
				}
				break;
			case KEY_DOWN:
				if((nth_node = g_queue_peek_nth(&hist_buffs,history_buffs_index-1)))
				{
					usr_in_buff = ((history_buffer_entry *)nth_node)->usr_in_buff;
					history_buffs_index--;
					move(getcury(stdscr),3);
					clrtoeol();
					printw("%s",usr_in_buff);
					end_index = strlen(usr_in_buff);
					cur_pos = end_index;
				}
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
				move(getcury(stdscr),getcurx(stdscr)+(end_index-cur_pos));
				argc = user_inp_dec(argv, usr_in_buff, start_sn, num_of_pSDAQ, pSDAQs_mem);
				user_com(argc, argv, start_sn, num_of_pSDAQ, pSDAQs_mem);
				printw("\n][ ");
				end_index = 0;
				cur_pos = 0;
				if(*usr_in_buff && !history_buffs_index)//make new entry in the history queue only if the current usr_in_buff is not empty and not used
				{
					g_queue_push_head(&hist_buffs, g_slice_alloc0(sizeof(history_buffer_entry)));
					usr_in_buff = ((history_buffer_entry *)g_queue_peek_head(&hist_buffs))->usr_in_buff;
					if(g_queue_get_length(&hist_buffs)>history_buff_length)
						history_buff_free_node(g_queue_pop_tail(&hist_buffs));
				}
				else
					for(int i=0;i<user_inp_buf_size;i++)
						usr_in_buff[i] = '\0';
				history_buffs_index = 0;
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
	g_queue_foreach(&hist_buffs, print_hist_buffs, NULL);
	g_queue_free_full(&hist_buffs,history_buff_free_node);//free the allocated space of the history buffers
	return;
}

int user_inp_dec(char **arg, char *usr_in_buff, unsigned int start_sn, unsigned char num_of_pSDAQ, pSDAQ_memory_space *pSDAQs_mem)
{
	unsigned char i=0;
	static char decode_buff[user_inp_buf_size];//assistance copy buffer, used instead of usr_in_buff to do not destroy the contents
	strcpy(decode_buff, usr_in_buff);
	arg[i] = strtok (decode_buff," ");
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
	buff_arr[0] = strtok (buff, "/");
	buff_arr[1] = strtok (NULL, "/");
	if(buff_arr[0] && buff_arr[1])
	{
		printw("\nyear= %d month = %d",atoi(buff_arr[0]),atoi(buff_arr[1]));
		if(atoi(buff_arr[0])<1900 || (atoi(buff_arr[1])>11 || !atoi(buff_arr[1])))
			return 1;
		memset(exp_date_dec,0,sizeof(struct tm));
		exp_date_dec->tm_year = atoi(buff_arr[0]) - 1900;
		exp_date_dec->tm_mon = atoi(buff_arr[1]) - 1;
	}
	return 0;
}

void user_com(unsigned int argc, char **argv, unsigned int start_sn, unsigned char num_of_pSDAQ, pSDAQ_memory_space *pSDAQs_mem)
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
					if(argv[2])
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
	}
	printw("\n  ????");
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




