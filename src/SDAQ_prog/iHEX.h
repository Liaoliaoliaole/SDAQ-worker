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
	IHEX_ERROR_INVALID_ARGUMENTS,
	IHEX_ERROR_CHECKSUM,
	IHEX_ERROR_NEWLINE,
	IHEX_ERROR_MAX_NUM = IHEX_ERROR_NEWLINE
};

typedef struct{
	GSList *mem_data_regions;
} mem_bin;

//Function that read a Intel hex file, decode the contents and populate the mem_table.
int iHEX_read_file(const char *file_path, mem_bin *mem_table);

//Function that decode an iHEX_Errors and print it.
void iHEX_error_print(unsigned int error_num);

#endif //iHEX_H
