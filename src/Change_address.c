#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>
#include <pthread.h> 

#include <linux/can.h>
#include <linux/can/raw.h>

#include "SDAQ_drv.h"
#include "Modes.h"


void Change_address(int socket_num, unsigned int serial_number, unsigned char new_address)
{
	printf("Not implemented\n");

}
