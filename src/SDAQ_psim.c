#define Stat_ID_Interval 200 //20 sec with base time 100ms 
#define Sync_Status_Interval 6// 2 min reset of InSync flag based on Stat_ID_Interval  

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
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
	num_of_pSDAQ = atoi(argv[2]);
	if(!num_of_pSDAQ || num_of_pSDAQ >= Parking_address)
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
		pthread_join(CAN_socket_RX_Thread_id[i], NULL);// wait pseudo_SDAQ thread to end
	
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
	struct ifreq ifr;
	struct sockaddr_can addr;	
	struct can_filter RX_filter;
	sdaq_can_id *can_filter_enc;
	int socket_num;
	//Variables for SDAQ_dev
	sdaq_can_id *id_dec;
	sdaq_set_new_addr *set_new_addr_dec;
	unsigned char dev_addr=Parking_address,status=0,raw_meas=0;
	unsigned int status_send_cnt=Stat_ID_Interval,sync_status_cnt=Sync_Status_Interval;
	float val = (float) arg.serial_number;//to be changed 
	//Variables for select
	struct timeval tv;
	fd_set ready_for_read;
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
	p_DeviceID_and_status(socket_num, dev_addr, arg.serial_number, status);
	p_DeviceInfo(socket_num, dev_addr, 16);
	while(run)
	{
		/* Set Watch SocketCAN to see when it's available for reading. */
		FD_ZERO(&ready_for_read); //init ready_for_read
		FD_SET(socket_num, &ready_for_read); //link Socket_num with ready_for_read
		tv.tv_sec = 0;
		tv.tv_usec = 100000;		
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
				id_dec = (sdaq_can_id *)&(frame_rx.can_id);
				if(id_dec->device_addr==dev_addr||id_dec->device_addr==Broadcast)
				{
					switch(id_dec->payload_type)
					{
						case Set_dev_address:
							set_new_addr_dec = (sdaq_set_new_addr *) frame_rx.data;
							if(set_new_addr_dec->dev_sn == arg.serial_number && set_new_addr_dec->new_address)
							{
								dev_addr = set_new_addr_dec->new_address;
								p_DeviceID_and_status(socket_num,dev_addr, arg.serial_number, status);
							}
							else if(!set_new_addr_dec->new_address)
								printf("Error at SDAQ_psim %2d: Invalid address (%d)\n",arg.serial_number,set_new_addr_dec->new_address);
							break;
						case Query_Dev_info: 
						case Change_SDAQ_baudrate:
							p_DeviceID_and_status(socket_num, dev_addr, arg.serial_number, status);
							p_DeviceInfo(socket_num, dev_addr, 16);
							break;
						case Start_command:
							status |= 1; //set run bit of status byte
							status_send_cnt = 0; //force a status message transmission 
							break;
						case Stop_command:
							status &= ~(1); //clear run bit of status byte
							status_send_cnt = 0; //force a status message transmission 
							break;	
						case Configure_Additional_data:
							raw_meas=frame_rx.data[0];
							break;
						case Synchronization_command:
							if(id_dec->device_addr==Broadcast)
							{	
								status |= 1<<In_sync;
								sync_status_cnt=Sync_Status_Interval;
							}
							break;
					}
				}
			}
		}
		else//select expired from Timeout
		{
			if(!status_send_cnt) //in every status_send_cnt zero a status message transmitted 
			{
				if(!sync_status_cnt) //in every status_send_cnt zero a the sync flag is reset
				{
				 	status &= ~(1<<In_sync);
				 	sync_status_cnt = Sync_Status_Interval;
				 }
				else
					sync_status_cnt--;
				p_DeviceID_and_status(socket_num, dev_addr, arg.serial_number, status);
				status_send_cnt = Stat_ID_Interval;
			}
			else
				status_send_cnt--;
			if(status & 0x01)//check run bit of status byte
			{
				for(int i=1;i<=16;i++)
				{
					val += i/100.0;
					if(val > 100.0)
						val=0.0;
					p_measure(socket_num, dev_addr, i, val, 0);
					if(raw_meas)
						p_measure_raw(socket_num, dev_addr, i, val, 0);	
				}
			}
		}
	}
	close(socket_num);
	return NULL;
}
