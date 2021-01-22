/*
File: iHex.c Implementation of Intel hex file related functions.
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "iHEX.h"

// General definition of the Intel HEX8 specification
enum _IHexDefinitions {
	// 768 should be plenty of space to read in a Intel HEX8 record
	IHEX_RECORD_BUFF_SIZE = 768,
	// Offsets and lengths of various fields in an Intel HEX8 record
	IHEX_COUNT_OFFSET = 1,
	IHEX_COUNT_LEN = 2,
	IHEX_ADDRESS_OFFSET = 3,
	IHEX_ADDRESS_LEN = 4,
	IHEX_TYPE_OFFSET = 7,
	IHEX_TYPE_LEN = 2,
	IHEX_DATA_OFFSET = 9,
	IHEX_CHECKSUM_LEN = 2,
	IHEX_MAX_DATA_LEN = 512,
	// Ascii hex encoded length of a single byte
	IHEX_ASCII_HEX_BYTE_LEN = 2,
	// Start code offset and value
	IHEX_START_CODE_OFFSET = 0,
	IHEX_START_CODE = ':'
};

// Intel Record Types
enum iHEX_RecordTypes {
	Data_Rec = 0, // Data Record
	End_of_file, // End of File Record
	Extended_address, // Extended Segment Address Record
	Start_Segmented_Address, // Start Segment Address Record
	Extended_Linear_Address, // Extended Linear Address Record
	Start_Linear_Address // Start Linear Address Record
};

// Structure to hold the fields of an Intel HEX8 record.
typedef struct {
	unsigned char dataLen; 						// The number of bytes of data stored in this record.
	unsigned short address; 					// The 16-bit address field.
	unsigned char type; 						// The Intel HEX8 record type of this record.
	unsigned char data[IHEX_MAX_DATA_LEN/2]; 	// The 8-bit array data field, which has a maximum size of 256 bytes.
	unsigned char checksum; 					// The checksum of this record.
} iHEX_Record;


			//--- Local Functions ---//
int iHEX_Record_enc(const char recordBuff[], iHEX_Record *rec);
//int Write_iHEX_Record(const iHEX_Record *iHEX_Record, FILE *out);
//void Print_iHEX_Record(const iHEX_Record *iHEX_Record);
unsigned char Checksum_iHEX_Record(const iHEX_Record *iHEX_Record);

int iHEX_read_file(const char *file_path, mem_bin *mem_table)
{
	iHEX_Record curr_iHEX_Record;
	FILE *fp;
	char buff[IHEX_RECORD_BUFF_SIZE];
	int last_error;

	if(!file_path || !mem_table)
		return EXIT_FAILURE;

	if(!(fp = fopen(file_path, "r")))
		return IHEX_ERROR_FILE;
	while(fgets(buff, sizeof(buff), fp))
	{
		for(int i=0; i<strlen(buff);i++)
		{
			if(buff[i] == '\n' || buff[i] == '\r')
				buff[i] = '0';
			else
				buff[i] = toupper(buff[i]);
		}
		if((last_error = iHEX_Record_enc(buff, &curr_iHEX_Record)))
		{
			iHEX_error_print(last_error);
			break;
		}
	}
	fclose(fp);

	return EXIT_SUCCESS;
}

void iHEX_error_print(int error_num)
{
	switch(error_num)
	{
		case IHEX_OK:
			puts("No error");
			break;
		case IHEX_ERROR_FILE:
			puts("Error while reading from or writing to a file.");
			break;
		case IHEX_ERROR_EOF:
			puts("Encountering end-of-file when reading from a file");
			break;
		case IHEX_ERROR_INVALID_RECORD:
			puts("Invalid record was read");
			break;
		case IHEX_ERROR_INVALID_ARGUMENTS:
			puts("Invalid arguments passed to function");
			break;
		case IHEX_ERROR_NEWLINE:
			puts("Encountering a newline with no record when reading from a file");
			break;
		default:
			puts("Unknown Error Code!!!");
	}
}

	//--- Local Functions Implementation ---//
int iHEX_Record_enc(const char recordBuff[], iHEX_Record *rec)
{
	unsigned char dataLen;
	char hexBuff[IHEX_ADDRESS_LEN+1];
	int dataCount, i;

	if(!recordBuff)
		return IHEX_ERROR_INVALID_ARGUMENTS;
	if(!(dataCount = strlen(recordBuff)))
		return IHEX_ERROR_NEWLINE;
	if(recordBuff[IHEX_START_CODE_OFFSET] != IHEX_START_CODE)//Check if recordBuff does not start with ':'
		return IHEX_ERROR_INVALID_RECORD;
	for(i=0; i < dataCount; i++)//Check for illegal characters
	{
		if((recordBuff[i] < '0' || recordBuff[i] > '9') && (recordBuff[i] < 'A' || recordBuff[i] > 'F') && recordBuff[i]!=':')
			return IHEX_ERROR_INVALID_RECORD;
	}

	/* Copy the ASCII hex encoding of the count field into hexBuff, convert it to a usable integer */
	strncpy(hexBuff, recordBuff+IHEX_COUNT_OFFSET, IHEX_COUNT_LEN);
	hexBuff[IHEX_COUNT_LEN] = '\0';
	dataLen = (int)strtoul(hexBuff, NULL, 16);
	
	// Size check for start code, count, address, and type fields
	if(dataCount != (1+IHEX_COUNT_LEN+IHEX_ADDRESS_LEN+IHEX_TYPE_LEN+dataLen*2+IHEX_CHECKSUM_LEN))
	{
		printf("dataCount=%d (%d)\n",dataCount, 1+IHEX_COUNT_LEN+IHEX_ADDRESS_LEN+IHEX_TYPE_LEN+dataLen*2+IHEX_CHECKSUM_LEN);
		return IHEX_ERROR_INVALID_RECORD;
	}
/*
	// Null-terminate the string at the first sign of a \r or \n
	for(int i = 0; i < dataCount; i++)
	{
		if(recordBuff[i] == '\r' || recordBuff[i] == '\n')
		{
			recordBuff[i] = 0;
			break;
		}
	}
	if (fgets(recordBuff, IHEX_RECORD_BUFF_SIZE, in) == NULL)
	{
		// In case we hit EOF, don't report a file error
		if (feof(in) != 0)
			return IHEX_ERROR_EOF;
		else
			return IHEX_ERROR_FILE;
	}
*/

	/* Copy the ASCII hex encoding of the address field into hexBuff, convert it to a usable integer */
	strncpy(hexBuff, recordBuff+IHEX_ADDRESS_OFFSET, IHEX_ADDRESS_LEN);
	hexBuff[IHEX_ADDRESS_LEN] = 0;
	rec->address = (uint16_t)strtoul(hexBuff, NULL, 16);

	/* Copy the ASCII hex encoding of the address field into hexBuff, convert it to a usable integer */
	strncpy(hexBuff, recordBuff+IHEX_TYPE_OFFSET, IHEX_TYPE_LEN);
	hexBuff[IHEX_TYPE_LEN] = 0;
	rec->type = (int)strtoul(hexBuff, NULL, 16);

	/* Loop through each ASCII hex byte of the data field, pull it out into hexBuff,
	 * convert it and store the result in the data buffer of the Intel HEX8 record */
	for (int i = 0; i < rec->dataLen; i++)
	{
		/* Times two i because every byte is represented by two ASCII hex characters */
		strncpy(hexBuff, recordBuff+IHEX_DATA_OFFSET+2*i, IHEX_ASCII_HEX_BYTE_LEN);
		hexBuff[IHEX_ASCII_HEX_BYTE_LEN] = 0;
		rec->data[i] = (uint8_t)strtoul(hexBuff, NULL, 16);
	}

	/* Copy the ASCII hex encoding of the checksum field into hexBuff, convert it to a usable integer */
	strncpy(hexBuff, recordBuff+IHEX_DATA_OFFSET+rec->dataLen*2, IHEX_CHECKSUM_LEN);
	hexBuff[IHEX_CHECKSUM_LEN] = 0;
	rec->checksum = (uint8_t)strtoul(hexBuff, NULL, 16);

	if (rec->checksum != Checksum_iHEX_Record(rec))
		return IHEX_ERROR_INVALID_RECORD;

	return IHEX_OK;
}

/*
int Write_iHEX_Record(const iHEX_Record *iHEX_Record, FILE *out)
{
	int i;
	// Check our record pointer and file pointer
	if(iHEX_Record == NULL || out == NULL)
		return IHEX_ERROR_INVALID_ARGUMENTS;
	//Check that the data length is in range
	if(iHEX_Record->dataLen > IHEX_MAX_DATA_LEN/2)
		return IHEX_ERROR_INVALID_RECORD;
	//Write the start code, data count, address, and type fields
	if(fprintf(out, "%c%2.2X%2.4X%2.2X", IHEX_START_CODE, iHEX_Record->dataLen, iHEX_Record->address, iHEX_Record->type) < 0)
		return IHEX_ERROR_FILE;
	//Write the data bytes
	for(i = 0; i < iHEX_Record->dataLen; i++)
		if(fprintf(out, "%2.2X", iHEX_Record->data[i]) < 0)
			return IHEX_ERROR_FILE;
	//Calculate and write the checksum field
	if(fprintf(out, "%2.2X\r\n", Checksum_iHEX_Record(iHEX_Record)) < 0)
		return IHEX_ERROR_FILE;

	return IHEX_OK;
}
*/
/*
void Print_iHEX_Record(const iHEX_Record *iHEX_Record)
{
	int i;
	printf("Intel HEX8 Record Type: \t%d\n", iHEX_Record->type);
	printf("Intel HEX8 Record Address: \t0x%2.4X\n", iHEX_Record->address);
	printf("Intel HEX8 Record Data: \t{");
	for(i = 0; i < iHEX_Record->dataLen; i++)
	{
		if(i+1 < iHEX_Record->dataLen)
			printf("0x%02X, ", iHEX_Record->data[i]);
		else
			printf("0x%02X", iHEX_Record->data[i]);
	}
	printf("}\n");
	printf("Intel HEX8 Record Checksum: \t0x%2.2X\n", iHEX_Record->checksum);
}
*/
unsigned char Checksum_iHEX_Record(const iHEX_Record *iHEX_Record)
{
	unsigned char checksum, *addr_dec = (unsigned char*)&(iHEX_Record->address);

	//Add the data count, type, address, and data bytes together
	checksum = iHEX_Record->dataLen;
	checksum += iHEX_Record->type;
	checksum += addr_dec[0];
	checksum += addr_dec[1];
	for (int i = 0; i < iHEX_Record->dataLen; i++)
		checksum += iHEX_Record->data[i];
	//Return the Two's complement of checksum
	return -(char)checksum;
}