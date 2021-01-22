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

#include <gmodule.h>

// All possible error codes the Intel HEX8 record utility functions may return.
enum iHEX_Errors {
	IHEX_OK = 0, 				// Error code for success or no error.
	IHEX_ERROR_FILE = 1, 			// Error code for error while reading from or writing to a file. You may check errno for the exact error if this error code is encountered.
	IHEX_ERROR_EOF = 2, 			// Error code for encountering end-of-file when reading from a file.
	IHEX_ERROR_INVALID_RECORD = 3, 	// Error code for error if an invalid record was read.
	IHEX_ERROR_INVALID_ARGUMENTS = 4, 	// Error code for error from invalid arguments passed to function.
	IHEX_ERROR_NEWLINE = 5, 		// Error code for encountering a newline with no record when reading from a file.
};

typedef struct{
	GSList *mem_data_regions;
} mem_bin;

//Function that read a Intel hex file, decode the contents and populate the mem_table.
int iHEX_read_file(const char *file_path, mem_bin *mem_table);

//Function that decode an iHEX_Errors and print it.
void iHEX_error_print(int error_num);