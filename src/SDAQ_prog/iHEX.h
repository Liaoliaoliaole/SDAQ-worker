/*
File: iHex.h Declaration of Intel hex file related functions.
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
#ifndef iHEX_H
#define iHEX_H

#include <gmodule.h>

extern void *DATA_PRINT_ON, *DATA_PRINT_OFF;

//All possible error codes.
enum iHEX_Errors {
	IHEX_OK = 0,
	IHEX_ERROR_FILE,
	IHEX_ERROR_EOF,
	IHEX_ERROR_INVALID_RECORD,
	IHEX_ERROR_ADDRESS_OUT_OF_RANGE,
	IHEX_ERROR_INVALID_ARGUMENTS,
	IHEX_ERROR_CHECKSUM,
	IHEX_ERROR_NEWLINE,
	IHEX_ERROR_MAX_NUM = IHEX_ERROR_NEWLINE
};

typedef struct memory_binary_str{
	unsigned short *cs,*ip;
	unsigned int *iep;
	GList *data_blks;
} rom_data;

//Struct for data of each node of GList data_reg
typedef struct rom_data_block_struct{
	unsigned int blk_addr, start_addr;
	GByteArray *blk_data;
} rom_data_block;

/*
 * Function that read a Intel hex from file or from memory.
 * Return IHEX_OK on success, or the iHEX_Errors code on failure.
*/
int iHEX_read_file(const char *file_path, rom_data *mem_table, unsigned char Print_error);
int iHEX_read_mem(const char *ihex_str, rom_data *mem_table);

//Function that free contents of rom_data
void free_rom_data(rom_data *ptr);
//Function that printing data_blks list, called from g_list_foreach().
void print_data_blks(gpointer data, gpointer print_flag);

//Function that decode an iHEX_Errors and return it as string.
const char * iHEX_strerror(unsigned int error_num);

#endif //iHEX_H
