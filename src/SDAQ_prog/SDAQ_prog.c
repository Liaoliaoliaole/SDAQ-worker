/*
Program: SDAQ_prog. A firmware downloading program for SDAQ-CAN Devices.
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
#define _GNU_SOURCE
#define VERSION "1.1" /*Release Version of SDAQ_prog*/

#define SDAQ_in_Bootloader 0x80
#define RETRY_LIMIT 2 /*Amount of failure CANBUS message receptions*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <zlib.h>

#include "../SDAQ_drv.h"
#include "../CANif_discovery.h"
#include "iHEX.h"
#include "../ver.h"

//Global variables.
static volatile _Bool run = TRUE;

//Application functions
GByteArray *SDAQ_flash_get_first_data_blk(rom_data *SDAQ_flash);
unsigned int SDAQ_flash_get_crc(rom_data *SDAQ_flash);
int SDAQ_prog(char *CAN_IF, unsigned char SDAQ_addr, unsigned char fw_dev_type, rom_data *SDAQ_flash, _Bool report);
void print_usage(char *prog_name);//Print the usage manual

//Handler function for quit signals
inline static void quit_signal_handler(int signum)
{
	run = FALSE;
}

int main(int argc, char *argv[])
{
	_Bool Silent = FALSE, fl_stdin = FALSE;
	char *iHEX_file_path = NULL, *CAN_if_name = NULL;
	unsigned char SDAQ_addr, fw_dev_type, *fw_dev_type_550X_ptr, *fw_dev_type_ptr, *fw_rev_ptr;
	GByteArray *SDAQ_flash_data;
	GString *iHEX_file_mem;
	rom_data SDAQ_flash = {0};
	//Option parsing variables
	int c, retval=EXIT_FAILURE;

	if(argc == 1)
	{
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	opterr = 1;
	while((c = getopt(argc, argv, "hvlsi")) != -1)
	{
		switch(c)
		{
			case 'h'://Help
				print_usage(argv[0]);
				exit(EXIT_SUCCESS);
			case 'v'://Version
				printf("Release: %s (%s)\nCompile Date: %s\nVer: "VERSION"\n", get_curr_git_hash(), get_release_date(), get_compile_date());
				exit(EXIT_SUCCESS);
			case 'l'://List of CAN-IF
				CANif_discovery();
				exit(EXIT_SUCCESS);
			case 's'://Silent
				Silent = TRUE;
				break;
			case 'i'://Interpreter
				fl_stdin = TRUE;
				break;
			case '?':
				exit(EXIT_FAILURE);
		}
	}
	if((argc - optind) < 2)
	{
		if(!argv[optind])
			fprintf(stderr, "CAN-IF argument Missing\n");
		if(!argv[optind+1])
			fprintf(stderr, "ADDRESS argument Missing\n");
		exit(EXIT_FAILURE);
	}
	if(strlen((CAN_if_name = argv[optind])) >= IFNAMSIZ)
	{
		fprintf(stderr, "CAN-IF name too big (>=%d)\n", IFNAMSIZ);
		exit(EXIT_FAILURE);
	}
	SDAQ_addr = atoi(argv[optind+1]);
	if(!SDAQ_addr || SDAQ_addr>=Parking_address)
	{
		fprintf(stderr, "Address is out of range!!!\n");
		exit(EXIT_FAILURE);
	}
	iHEX_file_path = argv[optind+2];

	//Link signal SIGINT, SIGTERM and SIGPIPE to quit_signal_handler
	signal(SIGINT, quit_signal_handler);
	signal(SIGTERM, quit_signal_handler);
	signal(SIGPIPE, quit_signal_handler);

	if(fl_stdin)
	{
		if(!Silent)
			printf("Enter the iHEX file in stdin and close it with EOF (Ctrl+D)\n");
		if(!(iHEX_file_mem = g_string_new(NULL)))
		{
			fprintf(stderr, "Memory Error!!!\n");
			exit(EXIT_FAILURE);
		}
		while((c = getchar()) != EOF)
			iHEX_file_mem = g_string_append_c(iHEX_file_mem, c);
		retval = iHEX_read(NULL, iHEX_file_mem->str, &SDAQ_flash, TRUE);
		g_string_free(iHEX_file_mem, TRUE);
	}
	else if(iHEX_file_path)
		retval = iHEX_read(iHEX_file_path, NULL, &SDAQ_flash, TRUE);
	else
		fprintf(stderr, "File path is undefined!!!\n");
	if(!retval)
	{
		if((SDAQ_flash_data = SDAQ_flash_get_first_data_blk(&SDAQ_flash)))
		{
			if(!((fw_dev_type_ptr = memmem(SDAQ_flash_data->data, SDAQ_flash_data->len, DEVID_indexer_str, DEVID_indexer_str_len)) ||
			     (fw_dev_type_550X_ptr = memmem(SDAQ_flash_data->data, SDAQ_flash_data->len, DEVID_indexer_str_550X, DEVID_indexer_str_550X_len))))
			{
				fprintf(stderr, "No device type reference found!!!\n");
				retval = EXIT_FAILURE;
			}
			if(!(fw_rev_ptr = memmem(SDAQ_flash_data->data, SDAQ_flash_data->len, REV_indexer_str, REV_indexer_str_len)))
			{
				fprintf(stderr, "No device revision reference found!!!\n");
				retval = EXIT_FAILURE;
			}
			fw_dev_type = fw_dev_type_ptr ? *(fw_dev_type_ptr + DEVID_indexer_str_len) : strtol((char*)(fw_dev_type_550X_ptr+DEVID_indexer_str_550X_len), NULL, 16);
			fw_rev_ptr += REV_indexer_str_len;
			if(fw_dev_type>SDAQ_MAX_DEV_NUM || !dev_type_str[fw_dev_type])
			{
				fprintf(stderr, "Firmware's Device type is Unknown!!!\n");
				retval = EXIT_FAILURE;
			}
			else
			{
				if(!Silent)
				{
					printf("SDAQ firmware for %s(%d), SW_Rev:%d, 0x%X - 0x%X (%d bytes), CRC:0x%X\n",
																 dev_type_str[fw_dev_type],
																 fw_dev_type,
																 *fw_rev_ptr,
																 iHEX_first_taddr(&SDAQ_flash),
																 iHEX_last_taddr(&SDAQ_flash),
																 iHEX_taddr_range(&SDAQ_flash),
																 SDAQ_flash_get_crc(&SDAQ_flash));
					//g_list_foreach(SDAQ_flash.data_blks, print_data_blks, DATA_PRINT_OFF);
				}
				retval = SDAQ_prog(CAN_if_name, SDAQ_addr, fw_dev_type, &SDAQ_flash, !Silent);
			}
		}
		else
		{
			fprintf(stderr, "Firmware does not have data!!!\n");
			retval = EXIT_FAILURE;
		}
	}
	free_rom_data(&SDAQ_flash);
	return retval;
}

GByteArray *SDAQ_flash_get_first_data_blk(rom_data *SDAQ_flash)
{
	GList *SDAQ_flash_blks_list;
	rom_data_block *SDAQ_flash_blk;

	if(!SDAQ_flash ||
	   !(SDAQ_flash_blks_list = SDAQ_flash->data_blks) ||
	   !(SDAQ_flash_blk = (rom_data_block *)SDAQ_flash_blks_list->data))
		return NULL;
	return SDAQ_flash_blk->blk_data;
}
//Return crc32 for first data_blk of SDAQ_flash.
unsigned int SDAQ_flash_get_crc(rom_data *SDAQ_flash)
{
	GByteArray *SDAQ_flash_data;

	if(!(SDAQ_flash_data = SDAQ_flash_get_first_data_blk(SDAQ_flash)))
		return 0;
	return crc32(0, SDAQ_flash_data->data, SDAQ_flash_data->len);
}

void print_usage(char *prog_name)
{
	const char preamp[] = {
	"Program: SDAQ_prog  Copyright (C) 12019-12021  Sam Harry Tzavaras\n"
    "\tThis program comes with ABSOLUTELY NO WARRANTY; for details see LICENSE.\n"
    "\tThis is free software, and you are welcome to redistribute it\n"
    "\tunder certain conditions; for details see LICENSE file.\n"
	};
	const char manual[] = {
		"CAN-IF: The name of the CANBUS interface.\n\n"
		"ADDRESS: A valid SDAQ address (Resolution:1..62).\n\n"
		"Options:\n"
		"           -h : Print help.\n"
		"           -v : Version.\n"
		"           -s : Silent mode.\n"
		"           -l : Print a list of the available CAN-IFs.\n"
		"           -i : In-line mode.\n"
		"\n"
	};
	printf("%s\nUsage: %s [Options] CAN-IF ADDRESS [Path to ROM File]\n\n%s",preamp, prog_name, manual);
	return;
}

/*
 * Function that copy PAGE_SIZE amount of bytes from rom_data's first block to buff_out, started from last_addr position.
 * Return: Zero if data remain in rom_data's first block, one if all are copied, or negative one on error;
 */
int SDAQ_get_page(rom_data *SDAQ_flash, unsigned char *buff_out, unsigned int last_addr)
{
	size_t page_size;
	GList *SDAQ_flash_blks_list;
	rom_data_block *SDAQ_flash_blk;
	GByteArray *blk_data;

	if(!SDAQ_flash || !buff_out)
		return -1;
	if(!(SDAQ_flash_blks_list = SDAQ_flash->data_blks))
		return -1;

	SDAQ_flash_blk = (rom_data_block*)SDAQ_flash_blks_list->data;
	blk_data = SDAQ_flash_blk->blk_data;
	if(last_addr < SDAQ_flash_blk->start_addr)
		return -1;
	last_addr -= SDAQ_flash_blk->start_addr;
	page_size = last_addr+PAGE_SIZE < blk_data->len ? PAGE_SIZE : blk_data->len - last_addr;
	memcpy(buff_out, blk_data->data+last_addr, page_size);
	if(page_size < PAGE_SIZE)
	{
		memset(buff_out+page_size , -1, PAGE_SIZE - page_size);
		return 1;
	}
	return 0;
}

int SDAQ_prog(char *CAN_IF_name, unsigned char SDAQ_addr, unsigned char fw_dev_type, rom_data *SDAQ_flash, _Bool report)
{
	enum SDAQ_prog_FSM_states{
		SDAQ_flash_erase,
		SDAQ_image_header,
		SDAQ_flash_prog,
		SDAQ_goto_app,
		SDAQ_prog_done
	};
	unsigned int SDAQ_flash_first_addr, SDAQ_flash_addr_range, SDAQ_flash_page_addr;
	unsigned char FSM_state=SDAQ_flash_erase, buff[PAGE_SIZE], retry_times=0;
	//Variables for Socket CAN
	struct timeval tv = {0};
	struct ifreq ifr = {0};
	struct sockaddr_can addr = {0};
	struct can_filter RX_filter = {0};
	struct can_frame frame_rx;
	int CAN_socket_num, RX_bytes;
	//SDAQ message Decoders
	sdaq_can_id *sdaq_id_dec;
	sdaq_status *status_dec;
	sdaq_bootloader_response *sdaq_bl_resp_dec;


	//Chech arguments for invalid entry.
	if(!CAN_IF_name || !SDAQ_flash || (!SDAQ_addr || SDAQ_addr>=Parking_address))
		return EXIT_FAILURE;

	//Check if SDAQ_flash have data block.
	if(!SDAQ_flash->data_blks)
		return EXIT_FAILURE;

	//CAN Socket Opening
	if((CAN_socket_num = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
	{
		perror("Error while opening socket");
		return EXIT_FAILURE;
	}
	//Link interface name to socket
	strcpy(ifr.ifr_name, CAN_IF_name);
	if(ioctl(CAN_socket_num, SIOCGIFINDEX, &ifr))
	{
		perror("CAN-IF");
		return EXIT_FAILURE;
	}
	/* Filter for CAN messages	-- SocketCAN Filters act as: <received_can_id> & mask == can_id & mask */
	//load filter's can_id member
	sdaq_id_dec = (sdaq_can_id *)&RX_filter.can_id;//Set encoder to filter.can_id
	memset(sdaq_id_dec, 0, sizeof(sdaq_can_id));
	sdaq_id_dec->flags = 4;//set the EFF
	sdaq_id_dec->protocol_id = PROTOCOL_ID; // Received Messages with protocol_id == PROTOCOL_ID
	sdaq_id_dec->payload_type = 0x80; //  Received Messages with payload_type & 0x80 == TRUE, aka Master <- SDAQ.
	sdaq_id_dec->device_addr = SDAQ_addr; // Receive only messages from SDAQ with address == SDAQ_addr.
	//load filter's can_mask member
	sdaq_id_dec = (sdaq_can_id *)&RX_filter.can_mask; //Set encoder to filter.can_mask
	memset(sdaq_id_dec, 0, sizeof(sdaq_can_id));
	sdaq_id_dec->flags = 4;//Received only messages with extended ID (29bit)
	sdaq_id_dec->protocol_id = -1; // Protocol_id field marked for examination
	sdaq_id_dec->payload_type = 0x80; // + The most significant bit of Payload_type field marked for examination.
	sdaq_id_dec->device_addr = -1; // Mark device_addr field to be examined.
	setsockopt(CAN_socket_num, SOL_CAN_RAW, CAN_RAW_FILTER, &RX_filter, sizeof(RX_filter));

	// Add timeout option to the CAN Socket
	tv.tv_sec = 2;//interval for timeout.
	tv.tv_usec = 0;
	setsockopt(CAN_socket_num, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

	//Bind CAN Socket to address
	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if(bind(CAN_socket_num, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("Error in socket bind");
		return EXIT_FAILURE;
	}

	//Initialize SDAQ related variables
	sdaq_id_dec = (sdaq_can_id *)&frame_rx.can_id;
	status_dec = (sdaq_status *)frame_rx.data;
	sdaq_bl_resp_dec = (sdaq_bootloader_response *)frame_rx.data;
	SDAQ_flash_first_addr = iHEX_first_taddr(SDAQ_flash);
	SDAQ_flash_page_addr = SDAQ_flash_first_addr;
	SDAQ_flash_addr_range = iHEX_taddr_range(SDAQ_flash);
	//SDAQ_prog's FSM
	SDAQ_goto(CAN_socket_num, SDAQ_addr, bootloader);
	if(report)
		printf("Attempt to enter Bootloader\n");
	while(run)
	{
		RX_bytes=read(CAN_socket_num, &frame_rx, sizeof(frame_rx));
		if(RX_bytes==sizeof(frame_rx))
		{
			switch(sdaq_id_dec->payload_type)//Check the received message type.
			{
				case Device_status:
					if(status_dec->dev_type != fw_dev_type)
					{
						fprintf(stderr, "Error: Firmware is not for this device type!!! (%u != %u)\n", fw_dev_type, status_dec->dev_type);
						SDAQ_goto(CAN_socket_num, SDAQ_addr, application);
						run = FALSE;
						break;
					}
					if(status_dec->status == SDAQ_in_Bootloader && FSM_state == SDAQ_flash_erase)
					{
						if(report)
						{
							printf("\tErase SDAQ's Flash... ");
							fflush(stdout);
						}
						SDAQ_erase_flash(CAN_socket_num, SDAQ_addr, (SDAQ_flash_first_addr-SDAQ_IMG_ADDR_OFFSET), iHEX_last_taddr(SDAQ_flash));
						retry_times = 0;
					}
					else if(FSM_state == SDAQ_goto_app)
					{
						FSM_state = SDAQ_prog_done;
						if(report)
							printf("Request SDAQ's info\n");
						QueryDeviceInfo(CAN_socket_num, SDAQ_addr);
						run = FALSE;
					}
					break;
				case Bootloader_reply:
					if(!sdaq_bl_resp_dec->error_code && !sdaq_bl_resp_dec->IAP_ret)
					{
						switch(FSM_state)
						{
							case SDAQ_flash_erase:
								if(report)
								{
									printf("Okay\n\tWrite SDAQ's Flash Header... ");
									fflush(stdout);
								}
								FSM_state = SDAQ_image_header;
							case SDAQ_image_header:
								if(report)
								{
									printf("Okay\n\tPrograming SDAQ's Flash [00%%]");
									fflush(stdout);
								}
								if(!SDAQ_write_header(CAN_socket_num, SDAQ_addr,
													  SDAQ_flash_first_addr,
													  SDAQ_flash_addr_range,
													  SDAQ_flash_get_crc(SDAQ_flash),
													  buff))
									SDAQ_Transfer_to_flash(CAN_socket_num, SDAQ_addr, (SDAQ_flash_first_addr-SDAQ_IMG_ADDR_OFFSET));
								else
								{
									fprintf(stderr, " Error at image header writing!!!\n");
									run = FALSE;
								}
								FSM_state = SDAQ_flash_prog;
								break;
							case SDAQ_flash_prog:
								if(SDAQ_get_page(SDAQ_flash, buff, SDAQ_flash_page_addr))
									FSM_state = SDAQ_goto_app;
								if(!SDAQ_write_page_buff(CAN_socket_num, SDAQ_addr, buff))
								{
									if(report)
									{
										printf("\b\b\b\b%02d%%]", 1+(100*(SDAQ_flash_page_addr-SDAQ_flash_first_addr))/SDAQ_flash_addr_range);
										fflush(stdout);
									}
									SDAQ_Transfer_to_flash(CAN_socket_num, SDAQ_addr, SDAQ_flash_page_addr);
									SDAQ_flash_page_addr += PAGE_SIZE;
								}
								else
								{
									fprintf(stderr, " Error at SDAQ flash's page loading!!!\n");
									run = FALSE;
								}
								break;
							case SDAQ_goto_app:
								if(report)
									printf("\nExit Bootloader\n");
								SDAQ_goto(CAN_socket_num, SDAQ_addr, application);
								break;
						}
					}
					else
					{
						fprintf(stderr, " Bootloader reply with Error!!!\n");
						SDAQ_goto(CAN_socket_num, SDAQ_addr, application);
						run = FALSE;
						break;
					}
					retry_times = 0;
					break;
				case Page_buff:
					if(memcmp(buff+(sdaq_id_dec->channel_num*frame_rx.can_dlc), frame_rx.data, frame_rx.can_dlc))
					{
						fprintf(stderr, " Error: SDAQ's flash verification!!!\n");
						run = FALSE;
					}
					retry_times = 0;
					break;
				default:
					retry_times++;
					if(retry_times >= RETRY_LIMIT)
					{
						fprintf(stderr, "Error: SDAQ's bootloader not responding!!!\n");
						run = FALSE;
					}
					break;
			}
		}
		else
		{
			retry_times++;
			if(retry_times >= RETRY_LIMIT)
			{
				fprintf(stderr, "Error: SDAQ not responding!!!\n");
				run = FALSE;
			}
		}
	}
	close(CAN_socket_num);//Close CAN_socket
	return FSM_state != SDAQ_prog_done ? EXIT_FAILURE : EXIT_SUCCESS;
}
