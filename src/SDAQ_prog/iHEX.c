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
	unsigned char data[IHEX_MAX_DATA_LEN/2]; 	// The 8-bit array data field.
	unsigned char checksum; 					// The checksum of this record.
} iHEX_Record;

static const char *iHEX_RecTypes_str[] = {
	"Data_Record",
	"End_of_file",
	"Extended_address",
	"Start_Segmented_Address",
	"Extended_Linear_Address",
	"Start_Linear_Address"
};

static const char *ERROR_strings[] = {
	"No error",
	"Error while reading from or writing to a file.",
	"Encountering end-of-file when reading from a file",
	"Invalid record was read",
	"Invalid arguments passed to function",
	"Checksum Error",
	"Encountering a newline with no record when reading from a file"
};

			//--- Local Functions ---//
int iHEX_Record_enc(char recordBuff[], iHEX_Record *rec);
int iHEX_rec_to_mem_bin(iHEX_Record *rec, mem_bin *mem_table);

//int Write_iHEX_Record(const iHEX_Record *iHEX_Record, FILE *out);

void Print_iHEX_Record(const iHEX_Record *iHEX_Record);
unsigned char Checksum_iHEX_Record(const iHEX_Record *iHEX_Record);

int iHEX_read_file(const char *file_path, mem_bin *mem_table)
{
	char buff[IHEX_RECORD_BUFF_SIZE];
	int last_error, line=1;
	iHEX_Record curr_iHEX_Record={0};
	FILE *fp;

	if(!file_path || !mem_table)
		return IHEX_ERROR_INVALID_ARGUMENTS;

	if(!(fp = fopen(file_path, "r")))
		return IHEX_ERROR_FILE;
	while(fgets(buff, sizeof(buff), fp))
	{
		if(!(last_error = iHEX_Record_enc(buff, &curr_iHEX_Record)))
		{
			iHEX_rec_to_mem_bin(&curr_iHEX_Record, mem_table);
			printf("%d%s\n", line, buff);
			Print_iHEX_Record(&curr_iHEX_Record);
		} 
		else 
			break;
		line++;
	}
	fclose(fp);
	return last_error;
}
/*
int iHEX_read_mem(const char *ihex_str, mem_bin *mem_table)
{
	int last_error, line=1;
	size_t ihex_str_size;
	iHEX_Record curr_iHEX_Record={0};
	char *iHEX_str_buff, *iHEX_str_line;

	if(!mem_table || !ihex_str || !(ihex_str_size = strlen(ihex_str)))
		return IHEX_ERROR_INVALID_ARGUMENTS;
	
	if(!(iHEX_str_buff = malloc(ihex_str_size*sizeof(char)+1)))
	{
		fprintf(stderr, "Memory error!!!\n");
		exit(EXIT_FAILURE);
	}
	strcpy(iHEX_str_buff, ihex_str);
	
	iHEX_str_line = strtok(iHEX_str_buff, "\n");
	while(iHEX_str_line)
	{
		if(!(last_error = iHEX_Record_enc(buff, &curr_iHEX_Record)))
		{
			append_iHEX_to_mem_bin(&curr_iHEX_Record, mem_table);
			printf("%d%s\n", line, buff);
			Print_iHEX_Record(&curr_iHEX_Record);
		} 
		else 
			break;
		line++;
		iHEX_str_line = strtok(NULL, "\n");
	}
	free(iHEX_str_buff);
	return last_error;
}
*/
const char * iHEX_strerror(unsigned int error_num)
{
	if(error_num<0 || error_num>IHEX_ERROR_MAX_NUM)
		return "Unknown Error Code!!!";
	else
		return ERROR_strings[error_num];
}

	//--- Local Functions Implementation ---//
int iHEX_rec_to_mem_bin(iHEX_Record *rec, mem_bin *mem_table)
{
	static unsigned int seg_offset=0;
	if(!rec || !mem_table)
		return IHEX_ERROR_INVALID_ARGUMENTS;
	switch(rec->type)
	{
		case Data_Rec:
			break;
		case Extended_address:
			break;
		case Extended_Linear_Address:
			break;
		case End_of_file:
			break;
		case Start_Segmented_Address:
			break;
		case Start_Linear_Address:
			break;			
	}
	return EXIT_SUCCESS;
}

unsigned short iHEX_field_to_val(char recordBuff[], size_t len)
{
	char hexBuff[IHEX_ADDRESS_LEN+1];

	strncpy(hexBuff, recordBuff, len);
	hexBuff[len] = '\0';
	return strtoul(hexBuff, NULL, 16);
}

int iHEX_Record_enc(char recordBuff[], iHEX_Record *rec)
{
	int dataCount;

	if(!recordBuff)
		return IHEX_ERROR_INVALID_ARGUMENTS;
	if(!(dataCount = strlen(recordBuff)))
		return IHEX_ERROR_NEWLINE;
	if(recordBuff[IHEX_START_CODE_OFFSET] != IHEX_START_CODE)//Check if recordBuff does not start with ':'
		return IHEX_ERROR_INVALID_RECORD;
	for(int i=0; i < dataCount; i++)//Check and condition the Record Buffer.
	{
		if(recordBuff[i]=='\r' || recordBuff[i]=='\n')//Replace <cr> and <lf> with null-termination
			recordBuff[i] = '\0';
		else if(!isxdigit(recordBuff[i]) && recordBuff[i]!=':')//Check for illegal characters
			return IHEX_ERROR_INVALID_RECORD;
	}
	if(!(dataCount = strlen(recordBuff)))
		return IHEX_ERROR_NEWLINE;
	rec->dataLen = iHEX_field_to_val(recordBuff+IHEX_COUNT_OFFSET, IHEX_COUNT_LEN);
	if(dataCount != (1+IHEX_COUNT_LEN+IHEX_ADDRESS_LEN+IHEX_TYPE_LEN+rec->dataLen*2+IHEX_CHECKSUM_LEN))//Check record size
		return IHEX_ERROR_INVALID_RECORD;
	for (int i=0; i < rec->dataLen; i++)//Convert data
		rec->data[i] = iHEX_field_to_val(recordBuff+IHEX_DATA_OFFSET+2*i, IHEX_ASCII_HEX_BYTE_LEN);
	rec->address = iHEX_field_to_val(recordBuff+IHEX_ADDRESS_OFFSET, IHEX_ADDRESS_LEN);
	rec->type = iHEX_field_to_val(recordBuff+IHEX_TYPE_OFFSET, IHEX_TYPE_LEN);
	rec->checksum = iHEX_field_to_val(recordBuff+IHEX_DATA_OFFSET+rec->dataLen*2, IHEX_CHECKSUM_LEN);
	if (rec->checksum != Checksum_iHEX_Record(rec))//Check if record is valid by checksum
		return IHEX_ERROR_CHECKSUM;
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

void Print_iHEX_Record(const iHEX_Record *iHEX_Record)
{
	int i;
	printf("\tRecord Type: %d (%s)\n", iHEX_Record->type, iHEX_RecTypes_str[iHEX_Record->type]);
	if(!iHEX_Record->type)
		printf("\tRecord Address: 0x%2.4X\n", iHEX_Record->address);
	if(iHEX_Record->dataLen)
	{
		printf("\tRecord Data Length: %d\n", iHEX_Record->dataLen);
		printf("\tRecord Data:{");
		for(i = 0; i < iHEX_Record->dataLen; i++)
			printf("0x%02X%s", iHEX_Record->data[i],i+1<iHEX_Record->dataLen?", ":"");
		printf("}\n");
	}
	printf("\tRecord Checksum: 0x%2.2X\n", iHEX_Record->checksum);
}

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
