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

//global variables 
unsigned char target=CMP_Serial_Numbers; //flag, used in SDAQentry_cmp and SDAQentry_find functions, switch the comparison target.
unsigned char TMR_exp=0;

//Local struct for SDAQ device entry 
struct SDAQentry {
    unsigned int serial_number;
	const char *dev_type;
    unsigned char address;
};

//local functions
struct SDAQentry* new_SDAQentry();
void free_SDAQentry(gpointer person);
void printf_SDAQentry(gpointer SDAQ_entry, gpointer data);
GSList * find_SDAQs(int socket_num, int scanning_time);//Construct a list with the SDAQs that is on the BUS, Sort by address.
GSList * find_SDAQs_inParking(GSList * head);//Construct a list with the SDAQs that be in parking, Sort by Serial number 
GSList * find_SDAQs_Conflicts(GSList * head);//Construct a list of lists with SDAQs that have the same address 
gint SDAQentry_cmp (gconstpointer a, gconstpointer b);
gint SDAQentry_find (gconstpointer a, gconstpointer b);

int Discover(int socket_num, opt_flags usr_flag)
{
	GSList *list_SDAQs=NULL,*list_Park=NULL,*list_with_conflict_address_lists=NULL;
	if(!(usr_flag.silent))
		printf("Scan the CANbus for %d sec ...\n",usr_flag.timeout);
	//Construct the list with the SDAQs that is on the BUS
	list_SDAQs = find_SDAQs(socket_num,usr_flag.timeout);//last argument is the scanning time
	if (list_SDAQs)
	{
		list_Park = find_SDAQs_inParking(list_SDAQs);//build list_Park with SDAQs in Parking mode 
		list_with_conflict_address_lists = find_SDAQs_Conflicts(list_SDAQs);//build list_with_conflict_address_lists  
		// print list_SDAQs
		printf("The scanning found %d SDAQ \n",g_slist_length(list_SDAQs));
		if(!(usr_flag.silent))
		{
			printf("==========  List of Discovered SDAQs   ==========\n");
			g_slist_foreach(list_SDAQs, printf_SDAQentry, NULL);
		}
		if(list_Park && list_with_conflict_address_lists == NULL)
		{
			// print list_Park
			if(g_slist_length(list_SDAQs)!=g_slist_length(list_Park))
			{
				printf("From them %d is/are in Parking\n",g_slist_length(list_Park));
				if(!(usr_flag.silent))
				{
					printf("==========  List of SDAQs in Parking   ==========\n");
					g_slist_foreach(list_Park, printf_SDAQentry, NULL);
				}
			}
			else
				printf("All of them is in Parking\n");
			printf("!!!!!!  Use mode 'autoconfig' to register them  !!!!!!\n");
		}
		
		if(list_with_conflict_address_lists)
		{
			// print list_with_conflict_address_lists
			printf("The following address/es found in multiple SDAQs\n");
			printf("==========  List of Conflict addresses  =========\n");
			g_slist_foreach(list_with_conflict_address_lists, printf_SDAQentry, NULL);
			printf("!!!!!!!  Use mode 'address' and correct them   !!!!!!!\n");
		}
		//free lists with only links
		g_slist_free(list_Park);
		g_slist_free(list_with_conflict_address_lists);
		//free lists with linked data
		g_slist_free_full(list_SDAQs, free_SDAQentry);

	}
	else
		printf("No SDAQ found\n");
	/*
	printf("\n===== SDAQ[3] =====\n\n");
    printf_SDAQentry(g_slist_nth_data(list_SDAQs, 3), NULL);
    
	printf("\n===== SDAQ[4] =====\n\n");
    printf_SDAQentry(g_slist_nth(list_SDAQs, 4)->data, NULL);
	*/
    return 0;
}

int Autoconfig(int socket_num, opt_flags usr_flag)
{
	GSList *list_SDAQs=NULL,*list_Park=NULL, *list_with_conflict_address_lists=NULL;
	list_SDAQs = find_SDAQs(socket_num,usr_flag.timeout);//last argument is the scanning time
	if (list_SDAQs)
	{
		list_Park = find_SDAQs_inParking(list_SDAQs);//build list_Park with SDAQs in Parking mode 
		list_with_conflict_address_lists=find_SDAQs_Conflicts(list_SDAQs); //build list_with_conflict_address_lists 
		if(!list_Park)//Check for no Parking SDAQs
			printf("All the founded SDAQs have valid address. Autoconfig Quit!!!\n");
		else if(list_with_conflict_address_lists) //Check for conflicts 
			printf("Address conflict found. Autoconfig Give Up!!!! \n");
		else //True Autoconfig -- to be made
		{
			printf("Not implemented\n");	
			
		}
		//free lists with only links
		g_slist_free(list_Park);
		g_slist_free(list_with_conflict_address_lists);
		//free lists with linked data
		g_slist_free_full(list_SDAQs, free_SDAQentry);
	}
	else
		printf("No SDAQ found\n");
	return 0;
}


// Allocates space for a new SDAQ entrance
struct SDAQentry* new_SDAQentry()
{
    struct SDAQentry *new_SDAQ = (struct SDAQentry *) g_slice_alloc(sizeof(struct SDAQentry));
    return new_SDAQ;
}

// frees the allocated space for struct SDAQentry and its data
void free_SDAQentry(gpointer SDAQentry_node) 
{
    	g_slice_free(struct SDAQentry,SDAQentry_node);
}   

// print function for SDAQentry node
void printf_SDAQentry(gpointer SDAQentry, gpointer arg_pass) 
{
	char address[12];
	if(((struct SDAQentry *) SDAQentry)->address!=Parking_address)
		sprintf(address,"%d",((struct SDAQentry *) SDAQentry)->address);
	else
	{
		if(!arg_pass)
			sprintf(address,"Parking");
		else
			return;
	}
	if(SDAQentry)
    	printf("%13s with S/N: %010d at Address: %s\n",((struct SDAQentry *) SDAQentry)->dev_type,
												  ((struct SDAQentry *) SDAQentry)->serial_number, 
												  address);
}

void timer_handler (int signum)
{
	 TMR_exp = 1;
	 return;
}
/*return a list with all the SDAQs on bus, sort by address*/
GSList * find_SDAQs(int socket_num, int scanning_time)
{
	//internal List with SDAQs
	GSList *ret_list=NULL;
	
	//CAN Socket related variables
	struct can_frame frame_rx;
	int RX_bytes;
	sdaq_can_id *id_dec;
	sdaq_status *status_dec;
	//Timers related Variables
	struct itimerval timer;//Scan Timeout
	
	//link signal SIGALRM to timer's handler
	signal(SIGALRM,timer_handler);
	
	//initialize timer expired time 
	memset (&timer, 0, sizeof(timer));
	timer.it_value.tv_sec = scanning_time;
	timer.it_value.tv_usec = 0;
	setitimer (ITIMER_REAL, &timer, NULL);
	
	//Query device info from every device
	QueryDeviceInfo(socket_num,0);
	while(!TMR_exp)
	{
		RX_bytes=read(socket_num, &frame_rx, sizeof(frame_rx));
		if(RX_bytes==sizeof(frame_rx))
		{
			id_dec = (sdaq_can_id *)&(frame_rx.can_id);
			status_dec = (sdaq_status *)&(frame_rx.data);
			if(id_dec->payload_type == Device_status)
			{	
				target = CMP_Serial_Numbers; // set SDAQentry_find and SDAQentry_cmp to sort by serial number
				// check if node with same Serial number exist in the list. if no, do store.
				if(g_list_find_custom((GList *)ret_list,(gconstpointer)&(status_dec->dev_sn),SDAQentry_find)==NULL)  
				{ 
					struct SDAQentry *new_sdaq = new_SDAQentry();
					if (new_sdaq) 
					{
						target = CMP_Addresses; // set SDAQentry_find and SDAQentry_cmp to sort by address
						// set SDAQ info data
						new_sdaq->serial_number = status_dec->dev_sn;
						new_sdaq->address = id_dec->device_addr; 
						new_sdaq->dev_type = dev_type_str[status_dec->dev_type];
						ret_list = g_slist_insert_sorted(ret_list, (gpointer) new_sdaq, SDAQentry_cmp);
					} 
					else
					{
						printf("Memory error\n");
						exit(1);
					}
				}
			}
		}
	}	
	return (GSList *) ret_list;
}
/*return a list with all the SDAQs that be in Parking on bus, sort by Serial number*/
GSList* find_SDAQs_inParking(GSList * head)
{
	GSList *t_lst = head, *ret_list=NULL;
	for(int i=g_slist_length(head);i;i--)//Run for all head nodes. 
	{
		target = CMP_Addresses; // Set SDAQentry_find and SDAQentry_cmp to sort by device address
		if(t_lst)
		{
			//look at the list t_lst (aka head, at first) for entrance with parking address
			t_lst = (GSList *)g_list_find_custom((GList *)t_lst,(gconstpointer) &(Parking_address),SDAQentry_find);
			if(t_lst)  
			{ 
				target = CMP_Serial_Numbers; // set SDAQentry_find and SDAQentry_cmp to sort by serial number
				ret_list = g_slist_insert_sorted(ret_list, (gpointer) t_lst->data, SDAQentry_cmp);//sort by serial number 
				t_lst = t_lst->next; //goto next node 
			}
			else
				break; //Break the for loop if no Parking_address node found.
		}
		else
			break;  //Break the for loop if end of list is reached.
	}	
	return (GSList *) ret_list;
}

GSList * find_SDAQs_Conflicts(GSList * head)  
{  
	GSList *ret_list=NULL; // function's return pointer	
	volatile GSList *look, *start = head; //start pointer pointing the first node on list. 
	target = CMP_Addresses; // Set SDAQentry_find and SDAQentry_cmp to work with device address
	unsigned char cur_address=0;
	if(g_slist_length(head)>1)
	{
		//Place start pointer the first SDAQ list node that does not be in parking
		while(start && ((((struct SDAQentry *)(start->data))->address)==Parking_address))
		{
			printf("new start look for parking\n");
			start = start->next; //move start to then next node
		}
		while(start->next)//Run until start pointer be one before the end of the list. 
		{
			printf("Start with @ %s with S/N %d and address%d\n",(((struct SDAQentry *)(start->data))->dev_type)
																,(((struct SDAQentry *)(start->data))->serial_number)
																,(((struct SDAQentry *)(start->data))->address));
			
			look = start->next;//look pointer pointing the next node after the start 
			while(look)//Run until look pointer hit the end of the nodes.
			{
				if(!SDAQentry_find(start->data, (gconstpointer)&(((struct SDAQentry *)(look->data))->address)))
				{
					cur_address = (((struct SDAQentry *)(look->data))->address);
					printf("Find conflict @ %s with S/N: %d and address: %d\n",(((struct SDAQentry *)(look->data))->dev_type)
																		   ,(((struct SDAQentry *)(look->data))->serial_number)
																		   ,cur_address);
				} 
				//Avoid look nodes in Parking 
				do{
					look = look->next; //move look pointer to next node
				}while(look && (((struct SDAQentry *)(look->data))->address)==Parking_address);
			}
			//Avoid start nodes with already checked address and nodes in Parking 
			do{
				start = start->next;//move start to then next node
			}while(start->next && (((((struct SDAQentry *)(start->data))->address)==cur_address)
							   ||  ((((struct SDAQentry *)(start->data))->address)==Parking_address)));
				  
		}
	}	
	return (GSList *) ret_list;
}

/*
	Comparing function used in g_slist_insert_sorted. 
	Controlled by target switch. 
*/
gint SDAQentry_cmp (gconstpointer a, gconstpointer b)
{
	switch(target)
	{
		case CMP_Serial_Numbers : 
			return (((struct SDAQentry *)a)->serial_number < ((struct SDAQentry *)b)->serial_number) ?  0 : 1;
		case CMP_Addresses : 
			return (((struct SDAQentry *)a)->address <= ((struct SDAQentry *)b)->address) ?  0 : 1;
		default : return 1;
	}
}

//Comparing function used in g_list_find_custom, comparing the SN field of the node and the arg inputs. Return 1 in case they differ. 
gint SDAQentry_find (gconstpointer node, gconstpointer arg)
{
	const int *arg_t = arg;
	struct SDAQentry *node_dec = (struct SDAQentry *) node;
	switch(target)
	{
		case CMP_Serial_Numbers:
			return node_dec->serial_number == (unsigned int) *arg_t ?  0 : 1;
		case CMP_Addresses:
			return node_dec->address == (unsigned char)*arg_t ?  0 : 1;
		default : return 1;
	}
}
