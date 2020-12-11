/*
File: Discovery_and_autoconfig.c.  Implementation of functions for modes "Discovery" and "autoconfig".
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

#define CMP_Serial_Numbers 1
#define CMP_Addresses 2
#define CMP_Addresses_no_parking 3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <gmodule.h>
#include <glib.h>
#include <sys/time.h>
#include <signal.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <sys/socket.h>

#include "SDAQ_drv.h"
#include "Modes.h"

//Local struct for SDAQ device entry
struct SDAQentry {
    unsigned int serial_number;
	const char *dev_type;
    unsigned char address;
};

//global variables
static volatile unsigned char Discover_and_autoconf_TMR_exp=1;

//Local functions
void free_SDAQentry(gpointer node);//used with g_slist_free_full to free the data of each node
void printf_SDAQentry(gpointer SDAQ_entry, gpointer data);
GSList * find_SDAQs(int socket_num, unsigned int scanning_time);//Construct a list with the SDAQs that is on the BUS, Sort by address.
GSList * find_SDAQs_inParking(GSList * head);//Construct a list with the SDAQs that be in parking, Sort by Serial number
GSList * find_SDAQs_Conflicts(GSList * head);//Construct a list of lists with SDAQs that have the same address
gint SDAQentry_cmp (gconstpointer a, gconstpointer b);// GFunc function used with g_slist_insert_sorted.
gint SDAQentry_find_address (gconstpointer node, gconstpointer arg);//Used in g_slist_find_custom, compare to find node with address == *arg
gint SDAQentry_find_serial_number (gconstpointer node, gconstpointer arg);//Used in g_slist_find_custom, compare to find node with serial_number == *arg
gint SDAQentry_find_autoconf (gconstpointer node, gconstpointer arg);//GFunc function used with g_slist_find_custom when is called from autoconfig.
// GFunc function used with g_slist_foreach. Arguments: SDAQ_new_address_list, pointer to socket number
void SDAQs_newaddress_list_to_SDAQs(gpointer SDAQentry, gpointer arg_pass);

int Discover(int socket_num, opt_flags *usr_flag)
{
	GSList *list_SDAQs=NULL,*list_Park=NULL,*list_conflicts=NULL;
	if(!(usr_flag->silent))
		printf("Scan the CANbus for %d sec ...\n",usr_flag->timeout);
	//Construct the list with the SDAQs that is on the BUS
	list_SDAQs = find_SDAQs(socket_num,usr_flag->timeout);//last argument is the scanning time
	if (list_SDAQs)
	{
		list_Park = find_SDAQs_inParking(list_SDAQs);//build list_Park with SDAQs in Parking mode
		list_conflicts = find_SDAQs_Conflicts(list_SDAQs);//build list_conflicts
		// print list_SDAQs
		printf("The discover found %d SDAQ ",g_slist_length(list_SDAQs));
		if(!(usr_flag->silent))
		{
			printf("\n==========  List of Discovered SDAQs   ==========\n");
			g_slist_foreach(list_SDAQs, printf_SDAQentry, NULL);
		}
		if(list_Park && list_conflicts == NULL)
		{
			// print list_Park
			if(g_slist_length(list_SDAQs)!=g_slist_length(list_Park))
			{
				printf("From them %d is/are in Parking\n",g_slist_length(list_Park));
				if(!(usr_flag->silent))
				{
					printf("==========  List of SDAQs in Parking   ==========\n");
					g_slist_foreach(list_Park, printf_SDAQentry, NULL);
				}
			}
			else
				printf("All of them is in Parking\n");
			printf("\tUse mode 'autoconfig' to register them!!!\n");
		}

		if(list_conflicts)
		{
			// print list_conflicts
			printf("\n!!!!!! Found %d device with address conflict !!!!!!\n",g_slist_length(list_conflicts));
			printf("==========  List of Conflict addresses  =========\n");
			g_slist_foreach(list_conflicts, printf_SDAQentry, NULL);
			printf("\n\tUse mode 'setaddress' and correct them!!!\n\n");
		}
		//free lists with only links
		g_slist_free(list_Park);
		g_slist_free(list_conflicts);
		//free lists with linked data
		g_slist_free_full(list_SDAQs, free_SDAQentry);

	}
	else
		printf("No SDAQ found\n");
    return EXIT_SUCCESS;
}

int Autoconfig(int socket_num, opt_flags *usr_flag)
{
	unsigned char addr_t=1;
	int ret_val=EXIT_SUCCESS,i,j;
	GSList *list_SDAQs=NULL,*list_Park=NULL, *list_conflicts=NULL;
	GSList *list_work=NULL;//list_work used as element pointer in addressing, and as list in verification.
	list_SDAQs = find_SDAQs(socket_num,usr_flag->timeout);//last argument is the scanning time
	if (list_SDAQs)
	{
		list_Park = find_SDAQs_inParking(list_SDAQs);//build list_Park with SDAQs in Parking mode
		list_conflicts=find_SDAQs_Conflicts(list_SDAQs); //build list_conflicts
		if(list_conflicts) //Check for conflicts
			printf("Address conflict found. Autoconfig Give Up!!!! \n");
		else if(!list_Park)//Check for no Parking SDAQs
		{
			if(!usr_flag->silent)
				printf("All found SDAQs have valid address.\nBye Bye!!\n");
		}
		else //True Autoconfig
		{
			list_work = list_Park;
			while(list_work)
			{
				if(!g_slist_find_custom(list_SDAQs,&addr_t,SDAQentry_find_address))
				{
					((struct SDAQentry *)list_work->data)->address=addr_t;
					list_work = list_work -> next;//next node with parking address
				}
				addr_t++;
			}
			if(!usr_flag->silent)
			{
				printf("Found %d SDAQs with Parking address\n",g_slist_length(list_Park));
				printf("New addresses send to SDAQs....\n");
			}
			//Send the new addresses to the SDAQs
			g_slist_foreach(list_Park, SDAQs_newaddress_list_to_SDAQs, &socket_num);
			/*Autoconfig verification*/
			if(!usr_flag->silent)
			{
				printf("-------Verification-------\n");
				printf("Scan the BUS for %d sec.\n",usr_flag->timeout);
			}
			list_work = find_SDAQs(socket_num,usr_flag->timeout);//last argument is the scanning time
			//check if the data->data of all the list_Park node is also in the list_work
			for(i=0,j=g_slist_length(list_Park);i<j;i++)
			{
				if(!g_slist_find_custom(list_work,g_slist_nth_data(list_Park,i),SDAQentry_find_autoconf))
				{
					if(!usr_flag->silent)
						printf("!!!!!! FAILURE !!!!!!!\n");
					ret_val = EXIT_FAILURE;
					break;
				}
			}
			//Success message to the user
			if(!usr_flag->silent && i==j)
				printf("SUCCESS\n");
		}
		//free lists with only links
		g_slist_free(list_Park);
		g_slist_free(list_conflicts);
		//free lists with linked data
		g_slist_free_full(list_work, free_SDAQentry);
		g_slist_free_full(list_SDAQs, free_SDAQentry);
	}
	else
	{
		if(!usr_flag->silent)
			printf("No SDAQ found\n");
	}
	return ret_val;
}

// frees the allocated space for struct SDAQentry and its data
void free_SDAQentry(gpointer SDAQentry_node)
{
    g_slice_free(struct SDAQentry,SDAQentry_node);
}

// print function for SDAQentry node
void printf_SDAQentry(gpointer SDAQentry, gpointer arg_pass)
{
	char address[5];
	if(((struct SDAQentry *) SDAQentry)->address!=Parking_address)
		sprintf(address,"%hhu",((struct SDAQentry *) SDAQentry)->address);
	else
		sprintf(address,"Park");
	if(SDAQentry)
    	printf("%13s with S/N: %010d at Address: %s\n",((struct SDAQentry *) SDAQentry)->dev_type,
												  ((struct SDAQentry *) SDAQentry)->serial_number,
												  address);
}

// GFunc function used with g_slist_foreach. Arguments: SDAQ_new_address_list, pointer to socket number
void SDAQs_newaddress_list_to_SDAQs(gpointer SDAQentry, gpointer arg_pass)
{
	//Configure with new address with arguments from the SDAQentry Node
	SetDeviceAddress(*((int*)arg_pass),((struct SDAQentry *) SDAQentry)->serial_number,
										((struct SDAQentry *) SDAQentry)->address);
	return;
}

void Discover_and_autoconf_timer_handler (int signum)
{
	Discover_and_autoconf_TMR_exp = 0;
	return;
}
/*return a list with all the SDAQs on bus, sort by address*/
GSList * find_SDAQs(int socket_num, unsigned int scanning_time)
{
	//Internal List with found SDAQs
	GSList *ret_list = NULL;
	struct SDAQentry *new_SDAQ_data;
	//CAN Socket and SDAQ related variables
	struct can_frame frame_rx;
	int RX_bytes;
	sdaq_can_id *id_dec = (sdaq_can_id *)&(frame_rx.can_id);
	sdaq_status *status_dec = (sdaq_status *)(frame_rx.data);
	//Timers related Variables
	struct itimerval timer;//Scan Timeout

	//link signal SIGALRM to timer's handler
	signal(SIGALRM, Discover_and_autoconf_timer_handler);

	//initialize timer expired time
	Discover_and_autoconf_TMR_exp = 1;
	memset (&timer, 0, sizeof(timer));
	timer.it_value.tv_sec = scanning_time;
	timer.it_value.tv_usec = 0;
	setitimer (ITIMER_REAL, &timer, NULL);

	//Query device info from every device
	QueryDeviceInfo(socket_num,Broadcast);
	while(Discover_and_autoconf_TMR_exp)
	{
		RX_bytes=read(socket_num, &frame_rx, sizeof(frame_rx));
		if(RX_bytes==sizeof(frame_rx))
		{
			if(id_dec->payload_type == Device_status)
			{
				//target = CMP_Serial_Numbers; // set SDAQentry_find and SDAQentry_cmp to sort by serial number
				// check if node with same Serial number exist in the list. if no, do store.
				if(!g_slist_find_custom(ret_list,&(status_dec->dev_sn),SDAQentry_find_serial_number))
				{
					// Allocates space for a new SDAQ entrance
					if((new_SDAQ_data = g_slice_alloc0(sizeof(struct SDAQentry))))
					{
						// set SDAQ info data
						new_SDAQ_data->serial_number = status_dec->dev_sn;
						new_SDAQ_data->address = id_dec->device_addr;
						new_SDAQ_data->dev_type = dev_type_str[status_dec->dev_type];
						ret_list = g_slist_insert_sorted(ret_list, new_SDAQ_data, SDAQentry_cmp);
					}
					else
					{
						fprintf(stderr,"Memory error\n");
						exit(EXIT_FAILURE);
					}
				}
			}
		}
	}
	return ret_list;
}
/*return a list with all the SDAQs nodes (from head) that have Parking address, sort by Serial number*/
GSList* find_SDAQs_inParking(GSList * head)
{
	GSList *t_lst = head, *ret_list=NULL;
	for(int i=g_slist_length(head);i;i--)//Run for all head nodes.
	{
		//target = CMP_Addresses; // Set SDAQentry_find and SDAQentry_cmp to sort by device address
		if(t_lst)
		{
			//look at the list t_lst (aka head, at first) for entrance with parking address
			t_lst = g_slist_find_custom(t_lst,(gconstpointer) &(Parking_address),SDAQentry_find_address);
			if(t_lst)
			{
				ret_list = g_slist_insert_sorted(ret_list, (gpointer) t_lst->data, SDAQentry_cmp);//sort by serial number
				t_lst = t_lst->next; //goto next node
			}
			else
				break; //Break the for loop if no Parking_address node found.
		}
		else
			break;  //Break the for loop if end of list is reached.
	}
	return ret_list;
}
/*return a list with all the SDAQs nodes (from head) that have same address (aka conflict)*/
GSList * find_SDAQs_Conflicts(GSList * head)
{
	GSList *ret_list=NULL; // function's return pointer
	GSList *look_ptr, *start_ptr = head; //start_ptr pointer pointing the first node on list.
	unsigned char cur_address=0;
	if(g_slist_length(head)>1)
	{
		//Place start_ptr pointer at first SDAQ list node that does not have parking address
		while(start_ptr->next && ((((struct SDAQentry *)(start_ptr->data))->address)==Parking_address))
			start_ptr = start_ptr->next; //move start_ptr to then next node
		while(start_ptr->next)//Run until start_ptr pointer be at the end node of the list.
		{
			ret_list = g_slist_append (ret_list, (gpointer) start_ptr->data); //append node that looked by start pointer in ret_list, as a possible conflict.
			look_ptr = start_ptr->next;//look_ptr pointer pointing the next node after the start_ptr
			while(look_ptr)//Run until look_ptr pointer be NULL. aka, pass from the last node.
			{
				//Check if the address field of the start_ptr node is the same with the node that point the look_ptr. aka if true, conflict found.
				if(((struct SDAQentry *)(start_ptr->data))->address == ((struct SDAQentry *)(look_ptr->data))->address)
				{
					ret_list = g_slist_append(ret_list, look_ptr->data);
					cur_address = (((struct SDAQentry *)(look_ptr->data))->address);
				}
				//Avoid, look_ptr points nodes with Parking address
				do{
					look_ptr = look_ptr->next; //move look_ptr pointer to next node
				}while(look_ptr && (((struct SDAQentry *)(look_ptr->data))->address)==Parking_address);
			}
			//delete last appending on ret_list if does not have conflict address. aka above check with look_ptr give false.
			if(g_slist_last(ret_list)->data == start_ptr->data)
				ret_list = g_slist_delete_link(ret_list,g_slist_last(ret_list));
			//Avoid, start_ptr points nodes with already checked address and nodes with Parking address
			do{
				start_ptr = start_ptr->next;//move start_ptr to then next node
			}while(start_ptr->next && (((((struct SDAQentry *)(start_ptr->data))->address)==cur_address)
							   ||  ((((struct SDAQentry *)(start_ptr->data))->address)==Parking_address)));
		}
	}
	return ret_list;
}

/*
	Comparing function used in g_slist_insert_sorted.
*/
gint SDAQentry_cmp (gconstpointer a, gconstpointer b)
{
	if(((struct SDAQentry *)a)->address != ((struct SDAQentry *)b)->address)
		return (((struct SDAQentry *)a)->address <= ((struct SDAQentry *)b)->address) ?  0 : 1;
	else
		return (((struct SDAQentry *)a)->serial_number <= ((struct SDAQentry *)b)->serial_number) ?  0 : 1;
}

/*
	Comparing function used in g_slist_find_custom, compare to find node with address == *arg
*/
gint SDAQentry_find_address (gconstpointer node, gconstpointer arg)
{
	const unsigned char *arg_t = arg;
	struct SDAQentry *node_dec = (struct SDAQentry *) node;
	return node_dec->address == (unsigned char)*arg_t ?  0 : 1;
}

/*
	Comparing function used in g_slist_find_custom, compare to find node with serial_number == *arg
*/
gint SDAQentry_find_serial_number (gconstpointer node, gconstpointer arg)
{
	const unsigned int *arg_t = arg;
	struct SDAQentry *node_dec = (struct SDAQentry *) node;
	return node_dec->serial_number == (unsigned int) *arg_t ?  0 : 1;
}

/*
	Comparing function used in g_slist_find_custom when is called in autoconfig
*/
gint SDAQentry_find_autoconf (gconstpointer node, gconstpointer arg)
{
	struct SDAQentry *node_dec = (struct SDAQentry *) node;
	struct SDAQentry *arg_dec = (struct SDAQentry *) arg;
	if(node_dec->serial_number == arg_dec->serial_number && node_dec->address == arg_dec->address && node_dec->dev_type == arg_dec->dev_type)
		return 0;
	else
		return 1;
}
