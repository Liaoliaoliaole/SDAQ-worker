#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <gmodule.h>
#include <glib.h>

#include <signal.h>
#include <pthread.h> 

#include <linux/can.h>
#include <linux/can/raw.h>

#include "SDAQ_drv.h"
#include "Modes.h"


//Local struct for SDAQ device entry 
struct SDAQentry {
    unsigned int serial_number;
    unsigned char address;
};

//local functions
struct SDAQentry* new_SDAQentry(unsigned int serial_number, unsigned char address);
void free_SDAQentry(gpointer person);
void printf_SDAQentry(gpointer SDAQ_entry, gpointer data);

gint serial_cmp (gconstpointer a, gconstpointer b)
{
	if(((struct SDAQentry *)a)->serial_number > ((struct SDAQentry *)b)->serial_number)
		return 1;
	else
		return 0;
}
gint address_cmp (gconstpointer a, gconstpointer b)
{
	if(((struct SDAQentry *)a)->address >= ((struct SDAQentry *)b)->address)
		return 1;
	else
		return 0;
}

void Discover(int socket_num)
{
	printf("Not implemented\n");
	
	GSList *list_SDAQs=NULL;

    // create a linked list of SDAQentries 
    for (int i = 0; i < 3; i++) 
    {
        struct SDAQentry *new_sdaq = new_SDAQentry(0, 0);
        if (new_sdaq) 
        {
            // set new_person data
            new_sdaq->serial_number = 10-i;
            new_sdaq->address = 10+1; 
			printf_SDAQentry(new_sdaq,NULL);
            //list_SDAQs = g_slist_insert_sorted(list_SDAQs, (gpointer) new_sdaq, serial_cmp);
            list_SDAQs = g_slist_insert_sorted(list_SDAQs, (gpointer) new_sdaq, address_cmp);
        } 
        else
        	printf("Memory error\n");
    }
    // print list
    printf("\n===== list of SDAQs =====\n");
    g_slist_foreach(list_SDAQs, printf_SDAQentry, NULL);
    /*
	printf("\n===== SDAQ[3] =====\n\n");
    printf_SDAQentry(g_slist_nth_data(list_SDAQs, 3), NULL);
    
	printf("\n===== SDAQ[4] =====\n\n");
    printf_SDAQentry(g_slist_nth(list_SDAQs, 4)->data, NULL);
	*/
    // free list
    g_slist_free_full(list_SDAQs, free_SDAQentry);

    return ;

}

// Allocates space for a new SDAQ entrance
struct SDAQentry* new_SDAQentry(unsigned int serial_number, unsigned char address)
{
    struct SDAQentry *new_SDAQ = (struct SDAQentry *) g_slice_alloc(sizeof(struct SDAQentry));
    if (new_SDAQ) 
    {
        new_SDAQ->serial_number = serial_number;
        new_SDAQ->address = address;
    }
    return new_SDAQ;
}

// frees the allocated space for struct SDAQentry and its data
void free_SDAQentry(gpointer SDAQentry_node) 
{
    	g_slice_free(struct SDAQentry,SDAQentry_node);
}   

// print function for SDAQentry node
void printf_SDAQentry(gpointer SDAQentry, gpointer unused) 
{
    if(SDAQentry)
    	printf("S/N: %d at Address: %d\n", ((struct SDAQentry *) SDAQentry)->serial_number, ((struct SDAQentry *) SDAQentry)->address);
}

