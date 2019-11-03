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


void Dev_info(int socket_num,unsigned char dev_addr)
{
	printf("Not implemented\n");

}
