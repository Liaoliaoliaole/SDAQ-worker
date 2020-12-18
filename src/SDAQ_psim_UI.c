/*
File: SDAQ_psim_UI. The user interface for the SDAQ_psim.
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

#define user_inp_buf_size 80
#define max_amount_of_user_arg 20
#define history_buffs_length 30

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
int user_inp_dec(char **argv, char *usr_in_buff);
//function for execution of user's command input
void user_com(unsigned int argc, char **argv, unsigned int start_sn, unsigned char num_of_pSDAQ, pSDAQ_memory_space *pSDAQs_mem);
//SDAQ_psim shell help, return 0 in success or 1 on failure
int shell_help();


void print_hist_buffs(gpointer data,gpointer user_data)
{
	static unsigned int i = 0;
	if(user_data)
		i = *(unsigned int *)user_data;
	history_buffer_entry *node_data = data;
	printf("buffer %d -> %s\n",i++,node_data->usr_in_buff);
}

//slice free function for history_buffs_nodes
void history_buff_free_node(gpointer node)
{
	g_slice_free(history_buffer_entry, node);
}

//Implementation of the user's Interface function
void user_interface(char *CAN_if, unsigned int start_sn, unsigned char num_of_pSDAQ, pSDAQ_memory_space *pSDAQs_mem)
{
	unsigned int end_index=0, cur_pos=0, key, argc, last_curx, history_buffs_index=0, retval;
	char *argv[max_amount_of_user_arg] = {NULL};
	GQueue *hist_buffs = g_queue_new();
	g_queue_push_head(hist_buffs, g_slice_alloc0(sizeof(history_buffer_entry)));
	gpointer nth_node = NULL;
	char *usr_in_buff = ((history_buffer_entry *)g_queue_peek_head(hist_buffs))->usr_in_buff;
	char temp_usr_in_buff[user_inp_buf_size];

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

			case 3 ://ctrl + c clear buffer
				move(getcury(stdscr),3);
				clrtoeol();
				end_index = 0;
				cur_pos = 0;
				for(int i=0;i<user_inp_buf_size;i++)
					usr_in_buff[i] = '\0';
				break;
			case 9 ://ctrl + i print CAN-if
				printw("\nSDAQ_psim interfacing with \"%s\"",CAN_if);
				printw("\n][ %s",usr_in_buff);
				break;
			case 17 ://ctrl + q
				SDAQ_psim_run = 0;
				break;
			case 12 : //ctrl + l
				clear();
				printw("][ %s",usr_in_buff);
				cur_pos = end_index;
				break;
			case '?' : //user request for help
				last_curx = getcurx(stdscr);
				retval = shell_help();
				refresh();
				clear();
				if(retval)
					printw("Terminal size too small to print help!!!\n");
				printw("][ %s",usr_in_buff);
				move(retval,last_curx);
				break;
			case KEY_UP:
				if((nth_node = g_queue_peek_nth(hist_buffs,history_buffs_index+1)))
				{
					if(!history_buffs_index)
						memcpy(temp_usr_in_buff, usr_in_buff, sizeof(char)*user_inp_buf_size);
					memset(usr_in_buff, '\0', sizeof(char)*user_inp_buf_size);
					strcpy(usr_in_buff, ((history_buffer_entry *)nth_node)->usr_in_buff);
					history_buffs_index++;
					move(getcury(stdscr),3);
					clrtoeol();
					printw("%s",usr_in_buff);
					end_index = strlen(usr_in_buff);
					cur_pos = end_index;
				}
				break;
			case KEY_DOWN:
				if((nth_node = g_queue_peek_nth(hist_buffs,history_buffs_index-1)))
				{
					memset(usr_in_buff, '\0', sizeof(char)*user_inp_buf_size);
					strcpy(usr_in_buff, ((history_buffer_entry *)nth_node)->usr_in_buff);
					history_buffs_index--;
					move(getcury(stdscr),3);
					clrtoeol();
					if(!history_buffs_index)
					{
						memcpy(usr_in_buff, temp_usr_in_buff, sizeof(char)*user_inp_buf_size);
						move(getcury(stdscr),3);
					}
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
					for(unsigned int i=cur_pos-1;i<=end_index;i++)
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
					for(unsigned int i=cur_pos;i<=end_index;i++)
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
			case '\r' :
			case '\n' ://return or enter : Command decode and execution
				usr_in_buff[end_index] = '\0';
				move(getcury(stdscr),getcurx(stdscr)+(end_index-cur_pos));
				argc = user_inp_dec(argv, usr_in_buff);
				user_com(argc, argv, start_sn, num_of_pSDAQ, pSDAQs_mem);
				printw("\n][ ");
				end_index = 0;
				cur_pos = 0;
				if(*usr_in_buff)//make new entry in the history queue only if the current usr_in_buff is not empty
				{
					g_queue_push_head(hist_buffs, g_slice_alloc0(sizeof(history_buffer_entry)));
					usr_in_buff = ((history_buffer_entry *)g_queue_peek_head(hist_buffs))->usr_in_buff;
					if(g_queue_get_length(hist_buffs)>history_buffs_length)
						history_buff_free_node(g_queue_pop_tail(hist_buffs));
				}
				history_buffs_index = 0;
				break;
			default : //normal key press
				if(isprint(key))
				{
					if(end_index<user_inp_buf_size-1)
					{	//check if cursor has moved from the user
						if(cur_pos<end_index)
						{	//roll right side of the buffer by one position
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
				else
				{
					printw("Special key = %d",key);
					printw("\n][ ");
				}
				break;
		}
	}
	endwin();
	//g_queue_foreach(hist_buffs, print_hist_buffs, NULL);
	g_queue_free_full(hist_buffs,history_buff_free_node);//free the allocated space of the history buffers
	return;
}

int user_inp_dec(char **arg, char *usr_in_buff)
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
	char *buff_arr[3];
	buff_arr[0] = strtok (buff, "/");
	buff_arr[1] = strtok (NULL, "/");
	buff_arr[2] = strtok (NULL, "/");
	if(buff_arr[0] && buff_arr[1] && buff_arr[2])
	{
		if(atoi(buff_arr[0])<2000 ||
		   atoi(buff_arr[1])>12 || !atoi(buff_arr[1]) ||
		   atoi(buff_arr[2])>31 || !atoi(buff_arr[2]))
			return 1;
		memset(exp_date_dec,0,sizeof(struct tm));
		exp_date_dec->tm_year = atoi(buff_arr[0]) - 1900;
		exp_date_dec->tm_mon = atoi(buff_arr[1]) - 1;
		exp_date_dec->tm_mday = atoi(buff_arr[2]);
	}
	else
		return 1;
	return 0;
}

void user_com(unsigned int argc, char **argv, unsigned int start_sn, unsigned char num_of_pSDAQ, pSDAQ_memory_space *pSDAQs_mem)
{
	unsigned char channel_dec = 0, point_dec, offset=1, unit_range_lo=0, unit_range_hi=0;
	unsigned int sn_dec;
	char *channel_str, str_buff[30];
	const char *unit_str_ptr;
	struct tm cal_date;
	if(argv[0])
	{
		if(!strcmp(argv[0],"code"))
		{
			if(argc == 2 && argv[1])// status if range is given
			{			
				sscanf(argv[1], "%hhd-%hhd",&unit_range_lo,&unit_range_hi);
				if(unit_range_lo!=unit_range_hi)
				{
					if(!unit_range_hi)
						unit_range_hi=unit_range_lo;
					for(int i=unit_range_lo; i<=unit_range_hi; i++)
					{
						unit_str_ptr = !unit_str[i]||!unit_str[i][0]?"Reserved ":unit_str[i];
						printw("\n   Unit_code: %i -> %s",i, unit_str_ptr);
						if(i<20)
							printw("(base)");
					}
					return;
				}
			}
		}
		else if(!strcmp(argv[0],"status"))
		{
			if(argc == 1 && !argv[1])// status for all pseudoSDAQs
			{
				for(int i=0; i<num_of_pSDAQ; i++)
				{
					pthread_mutex_lock(&SDAQs_mem_access[i]);
						printw("\n   pSDAQ %010d: Addr =",i+start_sn);
						if(pSDAQs_mem[i].address < Parking_address)
							printw(" %2d,",pSDAQs_mem[i].address);
						else
							printw("  P,");
						printw(" %-2d Channel(s),",pSDAQs_mem[i].number_of_channels);
						printw(" %9s,",pSDAQs_mem[i].status&0x01?"Measuring":"Stand-By");
						printw(" %2sSync,",pSDAQs_mem[i].status&(1<<In_sync)?"in":"no");
						printw(" %s",!pSDAQs_mem[i].pSDAQ_flags&(1<<disable)?"OnLine":"OffLine");
					pthread_mutex_unlock(&SDAQs_mem_access[i]);
				}
				return;
			}
			else if(argc == 2 && argv[1]) //status for specified pseudoSDAQ
			{
				sn_dec = atoi(argv[1]);
				if(sn_dec >= start_sn && sn_dec <= start_sn + num_of_pSDAQ-1)
				{
					pthread_mutex_lock(&SDAQs_mem_access[sn_dec - start_sn]);
						printw("\n   pSDAQ %010d: Address=",sn_dec);
						if(pSDAQs_mem[sn_dec - start_sn].address < Parking_address)
							printw("  %d,",pSDAQs_mem[sn_dec - start_sn].address);
						else
							printw(" onPark,");
						printw(" %d Channel%s,", pSDAQs_mem[sn_dec - start_sn].number_of_channels,
												 pSDAQs_mem[sn_dec - start_sn].number_of_channels>1?"s":"");
						printw(" %s,",pSDAQs_mem[sn_dec - start_sn].status&0x01?"Measuring":"Stand-By");
						printw(" %sSync",pSDAQs_mem[sn_dec - start_sn].status&(1<<In_sync)?"in":"no");
						for(int i=0;i<pSDAQs_mem[sn_dec - start_sn].number_of_channels;i++)
						{
							memset(&cal_date, 0, sizeof(struct tm));
							cal_date.tm_year = pSDAQs_mem[sn_dec - start_sn].ch_cal_date[i].year + 100; //100 = 2000-1900
							cal_date.tm_mon = pSDAQs_mem[sn_dec - start_sn].ch_cal_date[i].month - 1;
							cal_date.tm_mday =  pSDAQs_mem[sn_dec - start_sn].ch_cal_date[i].day;
							strftime (str_buff, sizeof(str_buff),"%Y/%m/%d",&cal_date);
							printw("\n\tCH%02d: Calibrated @ %s, period %03hhu month, Calibrated with %02d point, unit -> %s%s"
									,i+1
									,str_buff
									,pSDAQs_mem[sn_dec - start_sn].ch_cal_date[i].period
									,pSDAQs_mem[sn_dec-start_sn].ch_cal_date[i].amount_of_points
									,unit_str[pSDAQs_mem[sn_dec-start_sn].ch_cal_date[i].cal_units]
									,pSDAQs_mem[sn_dec-start_sn].ch_cal_date[i].cal_units<Unit_code_base_region_size?"(BASE)":"");
						}
					pthread_mutex_unlock(&SDAQs_mem_access[sn_dec - start_sn]);
					return;
				}
			}
			else if(argc == 3 && argv[1] && argv[2])// Status for pseudoSDAQ's Channel
			{
				sn_dec = atoi(argv[1]);
				if(strstr(argv[2],"CH") || strstr(argv[2],"ch"))
					channel_dec = atoi(argv[2]+2);
				if((sn_dec >= start_sn && sn_dec <= start_sn + num_of_pSDAQ-1) &&
				   (channel_dec > 0 && channel_dec <= pSDAQs_mem[sn_dec - start_sn].number_of_channels))
				{
					pthread_mutex_lock(&SDAQs_mem_access[sn_dec - start_sn]);
						memset(&cal_date, 0, sizeof(struct tm));
						cal_date.tm_year = pSDAQs_mem[sn_dec - start_sn].ch_cal_date[channel_dec-1].year + 100; //100 = 2000-1900
						cal_date.tm_mon = pSDAQs_mem[sn_dec - start_sn].ch_cal_date[channel_dec-1].month - 1;
						cal_date.tm_mday =  pSDAQs_mem[sn_dec - start_sn].ch_cal_date[channel_dec-1].day;
						strftime (str_buff, sizeof(str_buff),"%Y/%m/%d",&cal_date);
						printw("\n   CH%02d: Calibrated @ %s, period %3hhu month, Calibrated with %2d point, unit -> %s%s"
								,channel_dec
								,str_buff
								,pSDAQs_mem[sn_dec - start_sn].ch_cal_date[channel_dec-1].period
								,pSDAQs_mem[sn_dec-start_sn].ch_cal_date[channel_dec-1].amount_of_points
								,unit_str[pSDAQs_mem[sn_dec-start_sn].ch_cal_date[channel_dec-1].cal_units]
								,pSDAQs_mem[sn_dec-start_sn].ch_cal_date[channel_dec-1].cal_units<Unit_code_base_region_size?"(BASE)":"");

						for(int i=0;i<16;i++)
						{
							if(!i)
								printw("\n\t   Point #  |  Measure  | Reference |   Offset  |   Gain    |     C2    |     C3    |");
							printw("\n\t\t%2d  ",i);
							for(int j=0; j<6; j++)
							{
								printw("| %8.3g  ",pSDAQs_mem[sn_dec - start_sn].data_cal_values[channel_dec-1][i][j]);
								if(j==5)
									printw("|");
							}
						}
					pthread_mutex_unlock(&SDAQs_mem_access[sn_dec - start_sn]);
					return;
				}
			}
		}
		else if(!strcmp(argv[0],"get"))
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
							printw("\n\tCH%02d: Out_val = %.3f %s%s%s%s"
									  ,i+1
									  ,pSDAQs_mem[sn_dec-start_sn].out_val[i]
									  ,unit_str[pSDAQs_mem[sn_dec-start_sn].ch_cal_date[i].cal_units]
									  ,pSDAQs_mem[sn_dec-start_sn].noise & (1<<i) ? ", With Noise":""
									  ,pSDAQs_mem[sn_dec-start_sn].nosensor & (1<<i) ? ", No sensor":""
									  ,pSDAQs_mem[sn_dec-start_sn].out_of_range & (1<<i) ? ", Out of Range":"");
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
						if(strstr(argv[2],"off"))// Disable PseudoSDAQ
						{
							pthread_mutex_lock(&SDAQs_mem_access[sn_dec - start_sn]);
								pSDAQs_mem[sn_dec-start_sn].pSDAQ_flags |= 1<<disable;//Set pSDAQ offLine
								pSDAQs_mem[sn_dec-start_sn].status &= ~(0x01); // stop measuring
							pthread_mutex_unlock(&SDAQs_mem_access[sn_dec - start_sn]);
							printw("\n   SDAQ %010d: OFF-Line",sn_dec);
							return;
						}
						else if(strstr(argv[2],"on"))// Enable PseudoSDAQ
						{
							pthread_mutex_lock(&SDAQs_mem_access[sn_dec - start_sn]);
								pSDAQs_mem[sn_dec-start_sn].pSDAQ_flags &= ~(1<<disable);//Set pSDAQ onLine
								pSDAQs_mem[sn_dec-start_sn].status_send_cnt = 0;//force a send of status message
							pthread_mutex_unlock(&SDAQs_mem_access[sn_dec - start_sn]);
							printw("\n   SDAQ %010d: ON-Line",sn_dec);
							return;
						}
						else if(strstr(argv[2],"addr"))// Change address of PseudoSDAQ
						{
							if(argv[3])
							{
								if(strstr(argv[3],"park"))
								{
									pthread_mutex_lock(&SDAQs_mem_access[sn_dec - start_sn]);
										pSDAQs_mem[sn_dec-start_sn].address = Parking_address;
										pSDAQs_mem[sn_dec-start_sn].status &= ~(0x01); // stop measuring
										pSDAQs_mem[sn_dec-start_sn].status_send_cnt = 0;//force a send of status message
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
											pSDAQs_mem[sn_dec-start_sn].status &= ~(0x01); // stop measuring
											pSDAQs_mem[sn_dec-start_sn].status_send_cnt = 0;
										pthread_mutex_unlock(&SDAQs_mem_access[sn_dec - start_sn]);
										return;
									}
								}
							}
						}
						else if((channel_str = strstr(argv[2],"ch"))) // access specific PseudoSDAQ channel
						{
							channel_dec = atoi(channel_str+2);//channel number
							if(channel_dec >= 1 && channel_dec <= pSDAQs_mem[sn_dec - start_sn].number_of_channels && argv[3])
							{
								pthread_mutex_lock(&SDAQs_mem_access[sn_dec - start_sn]);
									if(!strcmp(argv[3],"date"))
									{
										if(argv[4])//Calibration date
										{
											if(!strcmp(argv[4],"now"))//if argument is "now"
											{
												time_t now = time(NULL);
												memcpy(&cal_date, gmtime(&now), sizeof(struct tm));
												pSDAQs_mem[sn_dec - start_sn].ch_cal_date[channel_dec-1].year = cal_date.tm_year - 100; //100 = 2000-1900
												pSDAQs_mem[sn_dec - start_sn].ch_cal_date[channel_dec-1].month = cal_date.tm_mon + 1; //+1 to get 12
												pSDAQs_mem[sn_dec - start_sn].ch_cal_date[channel_dec-1].day = cal_date.tm_mday;
												strftime (str_buff, sizeof(str_buff),"%Y/%m/%d",&cal_date);
												printw("\nSet Cal date for ch%d @ %s", channel_dec, str_buff);
												pSDAQs_mem[sn_dec-start_sn].pSDAQ_flags |= 1<<cal_dates_send;//Force resend of the cal_dates
											}
											else
											{
												if(!exp_date_dec_validator(&cal_date,argv[4]))
												{
													pSDAQs_mem[sn_dec - start_sn].ch_cal_date[channel_dec-1].year = cal_date.tm_year - 100; //100 = 2000-1900
													pSDAQs_mem[sn_dec - start_sn].ch_cal_date[channel_dec-1].month = cal_date.tm_mon + 1; //+1 to get 12
													pSDAQs_mem[sn_dec - start_sn].ch_cal_date[channel_dec-1].day = cal_date.tm_mday;
													pSDAQs_mem[sn_dec-start_sn].pSDAQ_flags |= 1<<cal_dates_send;//Force resend of the cal_dates
												}
												else
													printw("\n Argument of Date is invalid");
											}
										}
									}
									else if(!strcmp(argv[3],"period"))
									{
										if(argv[4])// Calibration period
										{
											sprintf(str_buff,"%i",atoi(argv[4]));//verification
											if(strstr(str_buff,argv[4]) && atoi(argv[4]) < 255)
											{
												pSDAQs_mem[sn_dec-start_sn].ch_cal_date[channel_dec-1].period = atoi(argv[4]);
												pSDAQs_mem[sn_dec-start_sn].pSDAQ_flags |= 1<<cal_dates_send;//Force resend of the cal_dates
											}
											else
												printw("\n Argument for Calibration Period is out of range");
										}
									}
									else if(!strcmp(argv[3],"points"))
									{
										if(argv[4])//amount of points
										{
											sprintf(str_buff,"%i",atoi(argv[4]));//verification
											if(strstr(str_buff,argv[4]) && atoi(argv[4])>= 0 && atoi(argv[4])<=16)
											{
												pSDAQs_mem[sn_dec-start_sn].ch_cal_date[channel_dec-1].amount_of_points = atoi(argv[4]);
												pSDAQs_mem[sn_dec-start_sn].pSDAQ_flags |= 1<<cal_dates_send;//Force resend of the cal_dates
											}
											else
												printw("\n Argument for amount of points is out of range (0..16)");
										}
									}
									else if(!strcmp(argv[3],"unit"))
									{
										if(argv[4])//Unit code
										{
											unsigned char unit_code = atoi(argv[4]);
											sprintf(str_buff,"%i",unit_code);
											if(strstr(str_buff,argv[4])&&unit_str[unit_code])
											{
												pSDAQs_mem[sn_dec-start_sn].ch_cal_date[channel_dec-1].cal_units = unit_code;
												pSDAQs_mem[sn_dec-start_sn].pSDAQ_flags |= 1<<cal_dates_send;//Force resend of the cal_dates
											}
											else
												printw("\n Argument of unit code is out of range");
										}
									}
									else if(!strcmp(argv[3],"nonoise"))
										pSDAQs_mem[sn_dec-start_sn].noise &= ~(1<<(channel_dec-1));
									else if(!strcmp(argv[3],"noise"))
										pSDAQs_mem[sn_dec-start_sn].noise |= 1<<(channel_dec-1);
									else if(!strcmp(argv[3],"nosensor"))
										pSDAQs_mem[sn_dec-start_sn].nosensor |= 1<<(channel_dec-1);
									else if(!strcmp(argv[3],"sensor"))
										pSDAQs_mem[sn_dec-start_sn].nosensor &= ~(1<<(channel_dec-1));
									else if(!strcmp(argv[3],"out"))
										pSDAQs_mem[sn_dec-start_sn].out_of_range |= 1<<(channel_dec-1);
									else if(!strcmp(argv[3],"in"))
										pSDAQs_mem[sn_dec-start_sn].out_of_range &= ~(1<<(channel_dec-1));
									else if(!strcmp(argv[3],"over"))
										pSDAQs_mem[sn_dec-start_sn].over_range |= 1<<(channel_dec-1);
									else if(!strcmp(argv[3],"under"))
										pSDAQs_mem[sn_dec-start_sn].over_range &= ~(1<<(channel_dec-1));
									else if((strstr(argv[3],"p") || strstr(argv[3],"P")) && argc == 6)//Set Point value
									{
										//check if value argument is number
										sprintf(str_buff,"%f",atof(argv[5]));
										if(strstr(str_buff,argv[5]))
										{
											if(strstr(argv[3],"point")||strstr(argv[3],"Point"))
												offset = strlen("point");
											point_dec = atoi(argv[3]+offset);
											if(point_dec>0 && point_dec<=16)
											{
												if(strstr(argv[4], "Meas") || strstr(argv[4], "meas"))
													pSDAQs_mem[sn_dec-start_sn].data_cal_values[channel_dec-1][point_dec-1][0] = atof(argv[5]);
												else if(strstr(argv[4], "Ref") || strstr(argv[4], "ref"))
													pSDAQs_mem[sn_dec-start_sn].data_cal_values[channel_dec-1][point_dec-1][1] = atof(argv[5]);
												else if(strstr(argv[4], "Offset") || strstr(argv[4], "offset"))
													pSDAQs_mem[sn_dec-start_sn].data_cal_values[channel_dec-1][point_dec-1][2] = atof(argv[5]);
												else if(strstr(argv[4], "Gain") || strstr(argv[4], "gain"))
													pSDAQs_mem[sn_dec-start_sn].data_cal_values[channel_dec-1][point_dec-1][3] = atof(argv[5]);
												else if(strstr(argv[4], "C2") || strstr(argv[4], "c2"))
													pSDAQs_mem[sn_dec-start_sn].data_cal_values[channel_dec-1][point_dec-1][4] = atof(argv[5]);
												else if(strstr(argv[4], "C3") || strstr(argv[4], "c3"))
													pSDAQs_mem[sn_dec-start_sn].data_cal_values[channel_dec-1][point_dec-1][5] = atof(argv[5]);
												else
													printw("\nUnknown point's value name");
											}
											else
												printw("\nPoint's Number is out of range (1..16)");
										}
										else
											printw("\nError: value argument is not a number");
									}
									else
									{
										if(argc == 4)
										{
											//check if the argument is number
											sprintf(str_buff,"%f",atof(argv[3]));
											if(strstr(str_buff,argv[3]))
											{
												pSDAQs_mem[sn_dec-start_sn].out_val[channel_dec-1] = atof(argv[3]);
												pSDAQs_mem[sn_dec-start_sn].nosensor &= ~(1<<(channel_dec-1));
											}
											else
												printw("\nError: out_value argument is not a number");
										}
										else
											printw("\n????");
									}
								pthread_mutex_unlock(&SDAQs_mem_access[sn_dec - start_sn]);
								return;
							}
						}
						else if(!strcmp(argv[2],"all"))
						{
							if(argv[3])
							{	
								pthread_mutex_lock(&SDAQs_mem_access[sn_dec - start_sn]);
									if(!strcmp(argv[3],"nonoise"))
										pSDAQs_mem[sn_dec-start_sn].noise = 0;
									else if(!strcmp(argv[3],"noise"))
										pSDAQs_mem[sn_dec-start_sn].noise = -1;
									else if(!strcmp(argv[3],"nosensor"))
										pSDAQs_mem[sn_dec-start_sn].nosensor = -1;
									else if(!strcmp(argv[3],"sensor"))
											pSDAQs_mem[sn_dec-start_sn].nosensor = 0;
									else if(!strcmp(argv[3],"out"))
											pSDAQs_mem[sn_dec-start_sn].out_of_range = -1;
									else if(!strcmp(argv[3],"in"))
											pSDAQs_mem[sn_dec-start_sn].out_of_range = 0;
									else if(!strcmp(argv[3],"over"))
											pSDAQs_mem[sn_dec-start_sn].over_range = -1;
									else if(!strcmp(argv[3],"under"))
											pSDAQs_mem[sn_dec-start_sn].over_range = 0;
									else if(!strcmp(argv[3],"unit"))
									{
										if(argv[4])//Unit code
										{
											unsigned char unit_code = atoi(argv[4]);
											sprintf(str_buff,"%i",unit_code);
											if(strstr(str_buff,argv[4])&&unit_str[unit_code])
											{
												for(int i=0;i<pSDAQs_mem[sn_dec-start_sn].number_of_channels;i++)
													pSDAQs_mem[sn_dec-start_sn].ch_cal_date[i].cal_units = unit_code;
												pSDAQs_mem[sn_dec-start_sn].pSDAQ_flags |= 1<<cal_dates_send;//Force resend of the cal_date message
											}
											else
												printw("\n Argument of unit code is out of range");
										}
									}
									else if(!strcmp(argv[3],"date"))
									{
										if(argv[4])//Calibration date
										{
											if(!strcmp(argv[4],"now"))//if argument is "now"
											{
												time_t now = time(NULL);
												memcpy(&cal_date, gmtime(&now), sizeof(struct tm));
												for(int i=0;i<pSDAQs_mem[sn_dec-start_sn].number_of_channels;i++)
												{
													pSDAQs_mem[sn_dec - start_sn].ch_cal_date[i].year = cal_date.tm_year - 100; //100 = 2000-1900
													pSDAQs_mem[sn_dec - start_sn].ch_cal_date[i].month = cal_date.tm_mon + 1; //+1 to get 12
													pSDAQs_mem[sn_dec - start_sn].ch_cal_date[i].day = cal_date.tm_mday;
													strftime (str_buff, sizeof(str_buff),"%Y/%m/%d",&cal_date);
													printw("\nSet Cal date for ch%02d @ %s", i+1, str_buff);
												}
												pSDAQs_mem[sn_dec-start_sn].pSDAQ_flags |= 1<<cal_dates_send;//Force resend of the cal_dates
											}
											else
											{
												if(!exp_date_dec_validator(&cal_date,argv[4]))
												{
													for(int i=0;i<pSDAQs_mem[sn_dec-start_sn].number_of_channels;i++)
													{
														pSDAQs_mem[sn_dec - start_sn].ch_cal_date[i].year = cal_date.tm_year - 100; //100 = 2000-1900
														pSDAQs_mem[sn_dec - start_sn].ch_cal_date[i].month = cal_date.tm_mon + 1; //+1 to get 12
														pSDAQs_mem[sn_dec - start_sn].ch_cal_date[i].day = cal_date.tm_mday;
														strftime (str_buff, sizeof(str_buff),"%Y/%m/%d",&cal_date);
														printw("\nSet Cal date for ch%02d @ %s", i+1, str_buff);
													}
													pSDAQs_mem[sn_dec-start_sn].pSDAQ_flags |= 1<<cal_dates_send;//Force resend of the cal_dates
												}
												else
													printw("\n Argument of Date is invalid");
											}
										}
									}
									else if(!strcmp(argv[3],"period"))
									{
										if(argv[4])// Calibration period
										{
											sprintf(str_buff,"%i",atoi(argv[4]));//verification
											if(strstr(str_buff,argv[4]) && atoi(argv[4]) < 255)
											{
												for(int i=0;i<pSDAQs_mem[sn_dec-start_sn].number_of_channels;i++)
													pSDAQs_mem[sn_dec-start_sn].ch_cal_date[i].period = atoi(argv[4]);
												pSDAQs_mem[sn_dec-start_sn].pSDAQ_flags |= 1<<cal_dates_send;//Force resend of the cal_dates
											}
											else
												printw("\n Argument for Calibration Period is out of range");
										}
									}
									else if(!strcmp(argv[3],"points"))
									{
										if(argv[4])//amount of points
										{
											sprintf(str_buff,"%i",atoi(argv[4]));//verification
											if(strstr(str_buff,argv[4]) && atoi(argv[4])>= 0 && atoi(argv[4])<=16)
											{
												for(int i=0;i<pSDAQs_mem[sn_dec-start_sn].number_of_channels;i++)
													pSDAQs_mem[sn_dec-start_sn].ch_cal_date[i].amount_of_points = atoi(argv[4]);
												pSDAQs_mem[sn_dec-start_sn].pSDAQ_flags |= 1<<cal_dates_send;//Force resend of the cal_dates
											}
											else
												printw("\n Argument for amount of points is out of range");
										}
									}
									else
									{	//check if the argument is number
										sprintf(str_buff,"%f",atof(argv[3]));
										if(strstr(str_buff,argv[3]))
										{
											pSDAQs_mem[sn_dec-start_sn].nosensor = 0;
											for(int i=0;i<pSDAQs_mem[sn_dec-start_sn].number_of_channels;i++)
												pSDAQs_mem[sn_dec-start_sn].out_val[i] = atof(argv[3]);
										}
										else if(!argv[4])
											printw("\nError: out_value argument is not a number");
										else
											printw("\n  ????");
									}
								pthread_mutex_unlock(&SDAQs_mem_access[sn_dec - start_sn]);
								return;
							}
						}
						else if(!strcmp(argv[2],"amount"))// amount of channels
						{
							if(argv[3])
							{
								if(atoi(argv[3])>0 && atoi(argv[3])<=16)
								{
									pthread_mutex_lock(&SDAQs_mem_access[sn_dec - start_sn]);
										pSDAQs_mem[sn_dec-start_sn].number_of_channels = atoi(argv[3]);
										pSDAQs_mem[sn_dec-start_sn].status_send_cnt = 0;//force a send of status message
										pSDAQs_mem[sn_dec-start_sn].pSDAQ_flags |= 1<<info_send;//Force resend of the Info message
										pSDAQs_mem[sn_dec-start_sn].pSDAQ_flags |= 1<<cal_dates_send;//Force resend of the cal_date message
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
	" KEYS:"
	"  KEY_UP    = Buffer up\n"
	"\tKEY_DOWN  = Buffer Down\n"
	"\tKEY_LEFT  = Cursor move left by 1\n"
	"\tKEY_RIGTH = Cursor move Right by 1\n"
	"\tCtrl + C  = Clear current buffer\n"
	"\tCtrl + L  = Clear screen\n"
	"\tCtrl + I  = print used CAN-if\n"
	"\tCtrl + Q  = Quit\n"
	" COMMANDS:\n"
	"\tcode #(-#) = print the unit string of the code (or range of codes)\n"
	"\tstatus (S/N) = Print status of S/N or all pSDAQs without S/N\n"
	"\tstatus S/N CH# = Print calibration points status of CH# at pSDAQ with S/N\n"
	"\tget (S/N) = Get the current outputs state\n"
	"\tset (S/N) on/off = Set a pseudo-SDAQ on or off line\n"
	"\tset (S/N) address (# || parking) = Set pSDAQ's address\n"
	"\tset (S/N) amount = Set the amount of channels. Range 1..16\n"
	"\tset (S/N) (ch# || all) [no]noise = [Re]Set random noise on channel(s)\n"
	"\tset (S/N) (ch# || all) [no]sensor = [Re]Set No sensor flag(s)\n"
	"\tset (S/N) (ch# || all) (out || in) = [Re]Set \"out of range\" flag(s)\n"
	"\tset (S/N) (ch# || all) (over || under) = [Re]Set \"Over-Range\" flag(s)\n"
	"\tset (S/N) (ch# || all) Real_val = Write value to Channel(s) output\n"
	"\tset (S/N) (ch# || all) date (now || YYYY/MM/DD) = Write Calibration Date\n"
	"\tset (S/N) (ch# || all) points # = Write Amount of Calibration points\n"
	"\tset (S/N) (ch# || all) period # = Write Calibration period\n"
	"\tset (S/N) (ch# || all) unit # = Write unit code\n"
	"\tset (S/N) ch# p(oint)# name Real_val = Set Channel's point value\n"
	"\t\tname := Meas, Ref, Offset, Gain, C2, C3\n"
};

//SDAQ_psim shell help
int shell_help()
{
	const int height = 32;
	const int width = 90;
	int starty = (LINES - height) / 2;	/* Calculating for a center placement */
	int startx = (COLS - width) / 2;	/* of the window		*/
	int key, scroll_lines=0;
	if(LINES>=height && COLS>=width)
	{
		WINDOW *help_win = newwin(height, width, starty, startx);
		keypad(help_win, TRUE);
		curs_set(0);//hide cursor
		scrollok(help_win, TRUE);
		mvwprintw(help_win, 1, 1, "%s", shell_help_str);
		mvwprintw(help_win, height-2, 1, " Press Ctrl+C to exit help");
		box(help_win, 0 , 0);
		wrefresh(help_win);
		do{
			key = getch();
			switch(key)
			{
				case KEY_UP:
					scroll_lines++;
					//wscrl(help_win, 1);
					wrefresh(help_win);
					break;
				case KEY_DOWN:
					scroll_lines--;
					//wscrl(help_win, -1);
					wrefresh(help_win);
					break;
			}
		}while(key!=3);
		wborder(help_win, ' ', ' ', ' ',' ',' ',' ',' ',' ');
		wclear(help_win);
		wrefresh(help_win);
		delwin(help_win);
		curs_set(1);//Show cursor
		return 0;
	}
	return 1;
}




