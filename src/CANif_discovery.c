/*
File: CAN-if_discovery. Implementation of Discovery function for CAN socket interfaces.
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
#include <unistd.h>
#include <ctype.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>

void CANif_discovery()
{
	struct ifaddrs *ifaddr, *ifa;

	if (getifaddrs(&ifaddr) == -1)
	{
		perror("getifaddrs");
		exit(EXIT_FAILURE);
	}
	/*Scan through the list ifaddr.
	Print ifa_name field of every node where the ifa_addr == NULL -- Possible make it better in future*/
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (ifa->ifa_addr == NULL)
			printf("%s\n",ifa->ifa_name);
	}
	freeifaddrs(ifaddr);
}
