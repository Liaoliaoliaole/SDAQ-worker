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
#define CHECK_ADDR_EQU(addr, node) addr - (node->start_addr - node->ext_addr) == node->blk_data->len? 1:0
#define CHECK_ADDR_RANGE(addr, node) addr - (node->start_addr - node->ext_addr) < node->blk_data->len? 1:0
#define IHEX_REC_LEN_CALC(len, index) (len - index) > IHEX_RECORD_DATA_SIZE ? IHEX_RECORD_DATA_SIZE : len - index

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <arpa/inet.h>

#include "iHEX.h"

// General definition of the Intel specification
enum _IHexDefinitions {
	// SIZE for data field in iHEX_record. Used in iHEX_write.
	IHEX_RECORD_DATA_SIZE = 16,
	// Offsets and lengths of various fields in an iHEX_record
	IHEX_COUNT_OFFSET = 1,
	IHEX_COUNT_LEN = 2,
	IHEX_ADDRESS_OFFSET = 3,
	IHEX_ADDRESS_LEN = 4,
	IHEX_START_ADDR_TYPE_LEN = 4,
	IHEX_EXTENTED_ADDR_TYPE_LEN = 2,
	IHEX_TYPE_OFFSET = 7,
	IHEX_TYPE_LEN = 2,
	IHEX_DATA_OFFSET = 9,
	IHEX_CHECKSUM_LEN = 2,
	IHEX_MAX_DATA_LEN = 512,
	// ASCII hex encoded length of a single byte
	IHEX_ASCII_HEX_BYTE_LEN = 2,
	// Start code offset and value
	IHEX_START_CODE_OFFSET = 0,
	IHEX_START_CODE = ':'
};

// Intel Record Types
enum iHEX_RecordTypes {
	Data_Rec = 0, // Data Record
	End_of_file, // End of File Record
	Extended_Segment_address, // Extended Segment Address Record
	Start_Segment_Address, // Start Segment Address Record
	Extended_Linear_Address, // Extended Linear Address Record
	Start_Linear_Address // Start Linear Address Record
};

// Structure to hold the fields of an Intel record.
typedef struct {
	unsigned char dataLen; 						// The number of bytes of data stored in this record.
	unsigned short address; 					// The 16-bit address field.
	unsigned char type; 						// The Intel  record type of this record.
	unsigned char data[IHEX_MAX_DATA_LEN/2]; 	// The 8-bit array data field.
	unsigned char checksum; 					// The checksum of this record.
} iHEX_Record;

static const char *iHEX_RecTypes_str[] = {
	"Data_Record",
	"End_of_file",
	"Extended_address",
	"Start_Segment_Address",
	"Extended_Linear_Address",
	"Start_Linear_Address"
};

static const char *ERROR_strings[] = {
	"No error",
	"Error while reading from or writing to a file.",
	"Encountering end-of-file when reading from a file",
	"Invalid record was read",
	"No EOF record found",
	"Address of record is out of range.",
	"Invalid arguments passed to function",
	"Checksum Error",
	"Encountering a newline with no record when reading from a file"
};
		//--- Variables that define extern ---//
gpointer DATA_PRINT_OFF = NULL, DATA_PRINT_ON = &DATA_PRINT_OFF;
			//--- Local Functions ---//
int iHEX_rec_to_rom_data(iHEX_Record *rec, rom_data *mem_table);
int iHEX_Record_dec(char recordBuff[], iHEX_Record *rec);
void Print_iHEX_Record(const iHEX_Record *iHEX_Record);
unsigned char Checksum_iHEX_Record(const iHEX_Record *iHEX_Record);
//GString in application append functions
void Append_Start_Segment_Address(GString *iHEX_file_data, unsigned short cs, unsigned short ip);
void Append_Start_Linear_Address(GString *iHEX_file_data, unsigned int iep);
void Append_Extended_Address(GString *iHEX_file_data, unsigned int addr, unsigned char rec_type);
void Append_Data(GString *iHEX_file_data, unsigned short addr, unsigned char *data, unsigned char len);

			//--- Implementation of Share Functions ---//
int iHEX_read(const char *iHEX_file_path, const char *iHEX_file_mem, rom_data *out_ptr, _Bool Print_error)
{
	int last_error = IHEX_OK, line=1, c;
	char *iHEX_str_line;
	GString *iHEX_file_data = NULL;
	iHEX_Record curr_iHEX_Record={0};
	FILE *iHEX_fp;

	if(!out_ptr || (iHEX_file_path && iHEX_file_mem))
		return IHEX_ERROR_INVALID_ARGUMENTS;

	//Check iHEX_file input source, and load it to iHEX_file_data.
	if(iHEX_file_path)
	{
		if(!(iHEX_fp = fopen(iHEX_file_path, "r")))
		{
			fprintf(stderr, "%s\n", iHEX_strerror(IHEX_ERROR_FILE));
			return IHEX_ERROR_FILE;
		}
		fseek(iHEX_fp, 0, SEEK_END);//Go to end of file.
		iHEX_file_data = g_string_sized_new(ftell(iHEX_fp));//Allocate iHEX_file_data.
		rewind(iHEX_fp);
		while((c = fgetc(iHEX_fp)) != EOF)
			iHEX_file_data = g_string_append_c(iHEX_file_data, c);
		fclose(iHEX_fp);
	}
	else if(iHEX_file_mem && strlen(iHEX_file_mem))
	{
		if(!(iHEX_file_data = g_string_new(iHEX_file_mem)))
		{
			fprintf(stderr, "Memory Error!!!\n");
			exit(EXIT_FAILURE);
		}
	}
	else
		return IHEX_ERROR_INVALID_ARGUMENTS;
	//Analyze and convert iHEX_file_data, and store it to rom_data
	iHEX_str_line = strtok(iHEX_file_data->str, "\n");
	while(iHEX_str_line && !out_ptr->file_ended)
	{
		if(!(last_error = iHEX_Record_dec(iHEX_str_line, &curr_iHEX_Record)))
		{
			if((last_error = iHEX_rec_to_rom_data(&curr_iHEX_Record, out_ptr)))
			{
				if(Print_error)
				{
					fprintf(stderr, "%s @ L%d -> %s\n", iHEX_strerror(last_error), line, iHEX_str_line);
					Print_iHEX_Record(&curr_iHEX_Record);
				}
				break;
			}
		}
		else
		{
			if(Print_error)
				fprintf(stderr, "%s @ L%d -> %s\n", iHEX_strerror(last_error), line, iHEX_str_line);
			break;
		}
		line++;
		iHEX_str_line = strtok(NULL, "\n");
	}
	g_string_free(iHEX_file_data, TRUE);
	if(!out_ptr->file_ended)
	{
		if(Print_error)
			fprintf(stderr, "%s\n", iHEX_strerror(IHEX_NO_EOF));
		return IHEX_NO_EOF;
	}
	return last_error;
}

int iHEX_write(rom_data *in_ptr, const char *iHEX_file_path, GString *iHEX_file_mem)
{
	int i, retval = EXIT_SUCCESS;
	unsigned int prev_ext_addr = 0;
	unsigned short addr;
	rom_data_block *curr_rom_blk;
	GList *curr_node = NULL;
	GByteArray *block_data = NULL;
	GString *iHEX_file_data = NULL;
	FILE *iHEX_fp;

	if(!in_ptr || (iHEX_file_path && iHEX_file_mem) || (!iHEX_file_path && !iHEX_file_mem))
		return IHEX_ERROR_INVALID_ARGUMENTS;
	if(!(curr_node = in_ptr->data_blks))
		return EXIT_FAILURE;
	if(iHEX_file_mem)
		iHEX_file_data = iHEX_file_mem;
	else //Build iHEX_file_data and add iHEX records to it.
		iHEX_file_data = g_string_new(NULL);
	while(curr_node)
	{
		curr_rom_blk = (rom_data_block *)curr_node->data;
		if(curr_rom_blk->ext_addr != prev_ext_addr)
		{
			if(curr_rom_blk->ext_addr & ~0xFFFFF)//Check if ext_addr is more than 20bits
				Append_Extended_Address(iHEX_file_data, curr_rom_blk->ext_addr, Extended_Linear_Address);
			else
				Append_Extended_Address(iHEX_file_data, curr_rom_blk->ext_addr, Extended_Segment_address);
			prev_ext_addr = curr_rom_blk->ext_addr;
		}
		block_data = curr_rom_blk->blk_data;
		addr = curr_rom_blk->start_addr - curr_rom_blk->ext_addr;
		for(i=0; i<block_data->len; i+=IHEX_RECORD_DATA_SIZE)
		{
			Append_Data(iHEX_file_data,	addr, block_data->data+i, IHEX_REC_LEN_CALC(block_data->len, i));
			addr+=IHEX_RECORD_DATA_SIZE;
		}
		curr_node = g_list_next(curr_node);
	}
	if(in_ptr->cs && in_ptr->ip)
		Append_Start_Segment_Address(iHEX_file_data, *in_ptr->cs, *in_ptr->ip);
	if(in_ptr->iep)
		Append_Start_Linear_Address(iHEX_file_data, *in_ptr->iep);
	g_string_append_printf(iHEX_file_data, ":00000001FF\n");//Append EOF record.
	if(iHEX_file_path)
	{
		if((iHEX_fp = fopen(iHEX_file_path, "w")))
		{
			for(i=0; i<iHEX_file_data->len; i++)
				fputc(iHEX_file_data->str[i], iHEX_fp);
			fclose(iHEX_fp);
		}
		else
			retval = EXIT_FAILURE;
		g_string_free(iHEX_file_data, TRUE);
	}
	return retval;
}

unsigned int iHEX_taddr_range(const rom_data *mem_table)
{
	unsigned int cnt = 0;
	GList *blks_list_node;
	rom_data_block *curr_blk_data, *prev_blk_data = NULL;

	if(!mem_table)
		return 0;
	blks_list_node = mem_table->data_blks;
	while(blks_list_node)
	{
		curr_blk_data = (rom_data_block*)blks_list_node->data;
		if(prev_blk_data)
		{
			if(prev_blk_data->blk_data->len != (curr_blk_data->start_addr - prev_blk_data->start_addr))
				cnt += curr_blk_data->start_addr - prev_blk_data->start_addr - prev_blk_data->blk_data->len;
		}
		cnt += curr_blk_data->blk_data->len;
		blks_list_node = g_list_next(blks_list_node);
		prev_blk_data = curr_blk_data;
	}
	return cnt;
}

unsigned int iHEX_first_taddr(const rom_data *mem_table)
{
	GList *blks_list_node;
	rom_data_block *fisrt_blk_data;

	if(!mem_table ||
	   !(blks_list_node = mem_table->data_blks) ||
	   !(fisrt_blk_data = (rom_data_block*)blks_list_node->data))
		return -1;
	return fisrt_blk_data->start_addr;
}

unsigned int iHEX_last_taddr(const rom_data *mem_table)
{
	GList *blks_list_node;
	rom_data_block *fisrt_blk_data;

	if(!mem_table ||
	   !(blks_list_node = mem_table->data_blks) ||
	   !(fisrt_blk_data = (rom_data_block*)blks_list_node->data))
		return -1;
	return fisrt_blk_data->start_addr + iHEX_taddr_range(mem_table)-1;
}

const char * iHEX_strerror(unsigned int error_num)
{
	if(error_num>IHEX_ERROR_MAX_NUM)
		return "Unknown Error Code!!!";
	else
		return ERROR_strings[error_num];
}

//Function that printing GList data_blks, called from g_list_foreach().
void print_data_blks(gpointer data, gpointer data_print_flag)
{
	rom_data_block *curr_node = (rom_data_block *)data;

	if(curr_node->blk_data && curr_node->blk_data->len)
	{
		printf("\nBlock:%u\n", curr_node->blk_index);
		printf("\tExtended address: 0x%08X\n", curr_node->ext_addr);
		printf("\tBlock size: %u bytes\n", curr_node->blk_data->len);
		printf("\tAddress range: 0x%08X-0x%08X\n", curr_node->start_addr, curr_node->start_addr+curr_node->blk_data->len-1);
		if(data_print_flag)
		{
			printf("\tBlock's data:\n\t{\n\t\t");
			for(unsigned int i=0; i<curr_node->blk_data->len; i++)
				printf("0x%02X%s", curr_node->blk_data->data[i], !((i+1)%8)?"\n\t\t":i!=curr_node->blk_data->len-1?", ":"");
			printf("\n\t}\n");
		}
	}
}

//Assisting function that freeing the rom_data_block, called by g_list_free_full.
void free_rom_data_block(gpointer data)
{
	GByteArray *blk_data_byte_array = ((rom_data_block *)data)->blk_data;
	if(blk_data_byte_array)
		g_byte_array_free(blk_data_byte_array, TRUE);
	g_slice_free(rom_data_block, data);
}
//Function that free contents of rom_data
void free_rom_data(rom_data *ptr)
{
	if(!ptr)
		return;
	if(ptr->cs)
		free(ptr->cs);
	if(ptr->ip)
		free(ptr->ip);
	if(ptr->iep)
		free(ptr->iep);
	g_list_free_full(ptr->data_blks, free_rom_data_block);
	ptr->data_blks = NULL;
	ptr->iep = NULL;
	ptr->ip = NULL;
	ptr->cs = NULL;
}

			//--- Implementation of Local Functions  ---//
int iHEX_rec_to_rom_data(iHEX_Record *rec, rom_data *rom_data_ptr)
{
	rom_data_block *new_rom_data_block = NULL, *curr_rom_data_block = NULL;

	if(!rec || !rom_data_ptr)
		return IHEX_ERROR_INVALID_ARGUMENTS;
	switch(rec->type)
	{
		case Data_Rec:
			if(!rec->dataLen)
				return IHEX_ERROR_INVALID_RECORD;
			if(rom_data_ptr->data_blks)
			{
				curr_rom_data_block = (rom_data_block *)(g_list_last(rom_data_ptr->data_blks)->data);
				if(CHECK_ADDR_RANGE(rec->address, curr_rom_data_block))
					return IHEX_ERROR_ADDRESS_OUT_OF_RANGE;
				if(!curr_rom_data_block->blk_data->len || CHECK_ADDR_EQU(rec->address, curr_rom_data_block))
				{
					if(!curr_rom_data_block->blk_data->len)
						curr_rom_data_block->start_addr = curr_rom_data_block->ext_addr + rec->address;
					curr_rom_data_block->blk_data = g_byte_array_append(curr_rom_data_block->blk_data,
																		rec->data,
																		rec->dataLen);
					break;
				}
			}
			new_rom_data_block = g_slice_new0(rom_data_block);
			if(curr_rom_data_block)
				new_rom_data_block->ext_addr = curr_rom_data_block->ext_addr;
			new_rom_data_block->start_addr = new_rom_data_block->ext_addr + rec->address;
			new_rom_data_block->blk_data = g_byte_array_new();
			new_rom_data_block->blk_data = g_byte_array_append(new_rom_data_block->blk_data,
															   rec->data,
															   rec->dataLen);
			new_rom_data_block->blk_index = g_list_length(rom_data_ptr->data_blks);
			rom_data_ptr->data_blks = g_list_append(rom_data_ptr->data_blks, new_rom_data_block);
			break;
		case Extended_Segment_address:
		case Extended_Linear_Address:
			if(rec->dataLen!=IHEX_EXTENTED_ADDR_TYPE_LEN || rec->address)
				return IHEX_ERROR_INVALID_RECORD;
			new_rom_data_block = g_slice_new0(rom_data_block);
			switch(rec->type)
			{
				case Extended_Segment_address:
					new_rom_data_block->ext_addr = ntohs(*(unsigned short*)rec->data)<<4;
					break;
				case Extended_Linear_Address:
					new_rom_data_block->ext_addr = ntohs(*(unsigned short*)rec->data)<<16;
					break;
			}
			new_rom_data_block->blk_data = g_byte_array_new();
			new_rom_data_block->blk_index = g_list_length(rom_data_ptr->data_blks);
			rom_data_ptr->data_blks = g_list_append(rom_data_ptr->data_blks, new_rom_data_block);
			break;
		case Start_Segment_Address:
		case Start_Linear_Address:
			if(rec->dataLen!=IHEX_START_ADDR_TYPE_LEN || rec->address)
				return IHEX_ERROR_INVALID_RECORD;
			switch(rec->type)
			{
				case Start_Segment_Address:
					if(!rom_data_ptr->cs && !rom_data_ptr->ip)
					{
						rom_data_ptr->cs = malloc(sizeof(unsigned short));
						rom_data_ptr->ip = malloc(sizeof(unsigned short));
						if(!rom_data_ptr->cs || !rom_data_ptr->ip)
						{
							fprintf(stderr, "Memory Error!!!\n");
							exit(EXIT_FAILURE);
						}
						*rom_data_ptr->cs = ntohs(((unsigned short *)(rec->data))[0]);
						*rom_data_ptr->ip = ntohs(((unsigned short *)(rec->data))[1]);
						break;
					}
					else
						return IHEX_ERROR_INVALID_RECORD;
				case Start_Linear_Address:
					if(!rom_data_ptr->iep)
					{
						rom_data_ptr->iep = malloc(sizeof(unsigned int));
						if(!rom_data_ptr->iep)
						{
							fprintf(stderr, "Memory Error!!!\n");
							exit(EXIT_FAILURE);
						}
						*rom_data_ptr->iep = ntohl(*((unsigned int *)(rec->data)));
						break;
					}
					else
						return IHEX_ERROR_INVALID_RECORD;
			}
			break;
		case End_of_file:
			if(rec->dataLen || rec->address)
				return IHEX_ERROR_INVALID_RECORD;
			rom_data_ptr->file_ended = TRUE;
			break;
	}
	return IHEX_OK;
}

unsigned short iHEX_field_to_val(char recordBuff[], size_t len)
{
	char hexBuff[IHEX_ADDRESS_LEN+1];
	strncpy(hexBuff, recordBuff, len);
	hexBuff[len] = '\0';
	return strtoul(hexBuff, NULL, 16);
}

int iHEX_Record_dec(char recordBuff[], iHEX_Record *rec)
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
	//Check record size
	if(dataCount != (1+IHEX_COUNT_LEN+IHEX_ADDRESS_LEN+IHEX_TYPE_LEN+rec->dataLen*2+IHEX_CHECKSUM_LEN))
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

void Append_Extended_Address(GString *iHEX_file_data, unsigned int addr, unsigned char rec_type)
{
	unsigned short iHEX_ext_addr;
	iHEX_Record iHEX_Rec;

	if(!iHEX_file_data)
		return;
	iHEX_Rec.dataLen = sizeof(unsigned short);
	iHEX_Rec.address = 0;
	switch(rec_type)
	{
		case Extended_Segment_address:
			iHEX_ext_addr = addr>>4;
			break;
		case Extended_Linear_Address:
			iHEX_ext_addr = addr>>16;
			break;
		default: return;
	}
	iHEX_Rec.type = rec_type;
	memcpy(iHEX_Rec.data, &iHEX_ext_addr, sizeof(unsigned short));
	iHEX_Rec.checksum = Checksum_iHEX_Record(&iHEX_Rec);
	g_string_append_printf(iHEX_file_data, ":%02X%04X%02X%04X%02X\n", iHEX_Rec.dataLen,
																	  iHEX_Rec.address,
																	  iHEX_Rec.type,
																	  iHEX_ext_addr,
																	  iHEX_Rec.checksum);
}

void Append_Start_Segment_Address(GString *iHEX_file_data, unsigned short cs, unsigned short ip)
{
	iHEX_Record iHEX_Rec;

	if(!iHEX_file_data)
		return;
	iHEX_Rec.dataLen = 2*sizeof(unsigned short);
	iHEX_Rec.address = 0;
	iHEX_Rec.type = Start_Segment_Address;
	memcpy(iHEX_Rec.data, &cs, sizeof(unsigned short));
	memcpy(iHEX_Rec.data+sizeof(unsigned short), &ip, sizeof(unsigned short));
	iHEX_Rec.checksum = Checksum_iHEX_Record(&iHEX_Rec);
	g_string_append_printf(iHEX_file_data, ":%02X%04X%02X%04X%04X%02X\n", iHEX_Rec.dataLen,
																		  iHEX_Rec.address,
																		  iHEX_Rec.type,
																		  cs,ip,
																		  iHEX_Rec.checksum);
}

void Append_Start_Linear_Address(GString *iHEX_file_data, unsigned int iep)
{
	iHEX_Record iHEX_Rec;

	if(!iHEX_file_data)
		return;
	iHEX_Rec.dataLen = sizeof(unsigned int);
	iHEX_Rec.address = 0;
	iHEX_Rec.type = Start_Linear_Address;
	memcpy(iHEX_Rec.data, &iep, sizeof(unsigned int));
	iHEX_Rec.checksum = Checksum_iHEX_Record(&iHEX_Rec);
	g_string_append_printf(iHEX_file_data, ":%02X%04X%02X%08X%02X\n", iHEX_Rec.dataLen,
																	  iHEX_Rec.address,
																	  iHEX_Rec.type,
																	  iep,
																	  iHEX_Rec.checksum);
}

void Append_Data(GString *iHEX_file_data, unsigned short addr, unsigned char *data, unsigned char len)
{
	iHEX_Record iHEX_Rec;

	if(!iHEX_file_data)
		return;
	iHEX_Rec.dataLen = len;
	iHEX_Rec.address = addr;
	iHEX_Rec.type = Data_Rec;
	memcpy(iHEX_Rec.data, data, sizeof(unsigned char)*len);
	iHEX_Rec.checksum = Checksum_iHEX_Record(&iHEX_Rec);
	g_string_append_printf(iHEX_file_data, ":%02X%04X%02X", iHEX_Rec.dataLen, iHEX_Rec.address,iHEX_Rec.type);
	for(int i=0; i<len; i++)
		g_string_append_printf(iHEX_file_data, "%02X", iHEX_Rec.data[i]);
	g_string_append_printf(iHEX_file_data, "%02X\n", iHEX_Rec.checksum);
}
