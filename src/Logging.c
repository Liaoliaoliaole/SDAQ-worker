/*
File: Logging.c, Implementation of function for mode "logging"
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


int Logging(int socket_num, unsigned char dev_addr, opt_flags *usr_flag)
{
	printf("Not implemented\n");
	return 0;
}
