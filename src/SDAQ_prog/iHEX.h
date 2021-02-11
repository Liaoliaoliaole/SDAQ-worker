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

//All possible error codes.
enum iHEX_Errors {
	IHEX_OK = 0,
	IHEX_ERROR_FILE,
	IHEX_ERROR_EOF,
	IHEX_ERROR_INVALID_RECORD,
	IHEX_NO_EOF,
	IHEX_ERROR_ADDRESS_OUT_OF_RANGE,
	IHEX_ERROR_INVALID_ARGUMENTS,
	IHEX_ERROR_CHECKSUM,
	IHEX_ERROR_NEWLINE,
	IHEX_ERROR_MAX_NUM = IHEX_ERROR_NEWLINE
};

typedef struct memory_binary_str{
	_Bool file_ended;
	unsigned short *cs,*ip;
	unsigned int *iep;
	GList *data_blks;
} rom_data;

//Struct for data of each node of GList data_reg
typedef struct rom_data_block_struct{
	unsigned int blk_index, ext_addr, start_addr;
	GByteArray *blk_data;
} rom_data_block;

//Used as values of print_data_blks()'s print_flag.
extern gpointer DATA_PRINT_ON, DATA_PRINT_OFF;

/*
 * Function that read a Intel hex from file or memory.
 * 	file_path or iHEX_file_mem set the source of Intel hex file.
 * 	  Only one source allowed to be set, otherwise an error will be occur.
 * 	If Print_error is set, at any error exception a message will be print at stderr.
 * 	Function will return IHEX_OK on success, or one of the iHEX_Errors codes at failure.
*/
int iHEX_read(const char *iHEX_file_path, const char *iHEX_file_mem, rom_data *mem_table, _Bool Print_error);
/*
 * Function that create and write a Intel hex with data from (rom_data) *mem_table.
 * 	file_path or iHEX_file_mem set the destination.
 * 	  Only one destination allowed to be set, otherwise an error will be occur.
 *    In case of call with iHEX_file_mem, The iHEX_file_mem must be initialized.
 * 	Function will return EXIT_SUCCESS on success, or EXIT_FAILURE at failure.
*/
int iHEX_write(rom_data *mem_table, const char *iHEX_file_path, GString *iHEX_file_mem);

//Function that count and return the amount of bytes for all blk_data in data_blks list part of rom_data, including gaped areas.
unsigned int iHEX_taddr_range(const rom_data *mem_table);
//Function that return the first address of the mem_table or -1 on error
unsigned int iHEX_first_taddr(const rom_data *mem_table);
//Function that return the last address of the mem_table or -1 on error
unsigned int iHEX_last_taddr(const rom_data *mem_table);

//Function that decode an iHEX_Errors and return it as string.
const char * iHEX_strerror(unsigned int error_num);

//Function that free contents of rom_data
void free_rom_data(rom_data *ptr);
//Function that printing data_blks list, called from g_list_foreach().
void print_data_blks(gpointer data, gpointer print_flag);
#endif //iHEX_H
