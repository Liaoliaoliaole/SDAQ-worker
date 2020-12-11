/*
File: SDAQ_psim_UI.h. Header file with functions declaration for SDAQ_psim_UI.c
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
#include "SDAQ_psim_types.h"
//string with the usage of the shell
extern const char shell_help_str[];
void user_interface(char *CAN_if, unsigned int start_sn, unsigned char num_of_pSDAQ, pSDAQ_memory_space *pSDAQs_mem);
