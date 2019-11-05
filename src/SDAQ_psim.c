#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h> 

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <linux/can.h>
#include <linux/can/raw.h>

//Include SDAQ Driver header
#include "SDAQ_drv.h"

//Global variables 
unsigned char run=1;
pthread_mutex_t thread_make_lock = PTHREAD_MUTEX_INITIALIZER;

struct thread_arguments_passer
{
	char *can_if_name;
	unsigned int serial_number;
};

//application functions
void print_usage(char *prog_name);
void * pseudo_SDAQ(void *varg_pt);//Thread function. Act as an pseudo_SDAQ.

void handle_sigint(int sig) 
{ 
    run=0;
} 


int main(int argc, char *argv[])
{
	unsigned int num_of_pSDAQ;
	//variables for threads
	pthread_t *CAN_socket_RX_Thread_id; 
	struct thread_arguments_passer thread_arg;
	
	if(argc <= 2|| argv[1]==NULL || argv[2]==NULL) 
	{
		print_usage(argv[0]);
		exit(1);
	}
	
	//sanitize, decode and copy the CAN-if name. 
	thread_arg.can_if_name=argv[1];
	if(!(num_of_pSDAQ = atoi(argv[2])))
	{
		printf("Amount of pseudo_SDAQ is invalid\n");
		exit(1);
	}
	
	//mount signal SIGINT to the signal handler
	signal(SIGINT, handle_sigint);
	
	CAN_socket_RX_Thread_id = malloc(sizeof(CAN_socket_RX_Thread_id)*num_of_pSDAQ); //allocate memory for the threads tags
	for(int i=0;i<num_of_pSDAQ;i++)
	{
		pthread_mutex_lock(&thread_make_lock);
		thread_arg.serial_number=i+1;
		pthread_create(&CAN_socket_RX_Thread_id[i], NULL, pseudo_SDAQ, &thread_arg);
	}
	
	
	while(run)
		sleep(1);

	for(int i=0;i<num_of_pSDAQ;i++)
		pthread_cancel(CAN_socket_RX_Thread_id[i]);// cancel pseudo_SDAQ threads
	
	free(CAN_socket_RX_Thread_id);
	return 0;
}


void print_usage(char *prog_name)
{
	printf("\nUsage: %s CAN-IF [Amount of pseudo_SDAQ Devices] \n\n",prog_name);
	return;
}

void * pseudo_SDAQ(void *varg_pt)//Thread function. Act as an pseudo_SDAQ.
{
	
	struct thread_arguments_passer arg;
	memcpy(&arg, varg_pt, sizeof(arg));//copy *varg_pt to arg (struct thread_arguments_passer)
	pthread_mutex_unlock(&thread_make_lock);//Unlock threading making 
	//Variables for Socket CAN
	struct can_frame frame_rx;
	int RX_bytes;
	struct timeval tv;
	struct ifreq ifr;
	struct sockaddr_can addr;	
	struct can_filter RX_filter;
	sdaq_can_id *can_filter_enc;
	int socket_num;
	//Variables for SDAQ_dev
	sdaq_can_id *id_dec;
	sdaq_set_new_addr *set_new_addr_dec;
	unsigned char dev_addr=Parking_address,status=0;

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
	tv.tv_sec = 20;
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
	
	while(1)
	{
		p_DeviceID_and_status(socket_num,dev_addr, arg.serial_number, status);
		RX_bytes=read(socket_num, &frame_rx, sizeof(frame_rx));
		if(RX_bytes==sizeof(frame_rx))
		{
			id_dec = (sdaq_can_id *)&(frame_rx.can_id);
			if(id_dec->device_addr==dev_addr||id_dec->device_addr==0)
			{
				if(id_dec->payload_type==Set_dev_address)
				{
					set_new_addr_dec = (sdaq_set_new_addr *) frame_rx.data;
					if(set_new_addr_dec->dev_sn == arg.serial_number && set_new_addr_dec->new_address)
						dev_addr = set_new_addr_dec->new_address;
					else if(!set_new_addr_dec->new_address)
					{
						printf("Error: Set_dev_address received on pSDAQ with Serial number %d but with invalid address\n",arg.serial_number);
					}
				}
			}
		}
	}	
}
