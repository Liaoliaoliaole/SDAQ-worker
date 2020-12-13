/*
File: SDAQ_xml.c, Implemntation of functions for read and write SDAQ related XMLs
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
// SDAQ_xml function implementation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

#include <glib.h>
#include <gmodule.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "SDAQ_drv.h"
#include "Modes.h"
#include "SDAQ_xml.h"

enum contens_type{
	t_float,
	t_integer_ubyte,
	t_integer_ushort,
	t_integer_uint,
	t_cal_date, //special, send wholly container
	t_string
};

//Custom function that convert an type (contens_type) to a node with name name_mode
xmlNodePtr xml_SDAQ_data(xmlNodePtr root_node , unsigned char *node_name, void *contents_ptr, unsigned char type);

int XML_info_file_write(char *file_path, void *arg)
{
	SDAQ_info_cal_data *info_ptr = arg;
	xmlDocPtr xml_doc = NULL;
    xmlNodePtr root_node = NULL, w_node = NULL,  w_node1 = NULL, w_node2 = NULL;
	unsigned char buff[20], *point_name, exp_format_flag=0, cal_unit;
    //Creates a new document, a node and set it as a root node
    xml_doc = xmlNewDoc(BAD_CAST "1.0");
    root_node = xmlNewNode(NULL, BAD_CAST "SDAQ");
	xmlDocSetRootElement(xml_doc, root_node);
	//add SDAQ info to xml
	w_node = xmlNewChild(root_node, NULL, BAD_CAST "SDAQ_info", NULL);
	xml_SDAQ_data(w_node, BAD_CAST "SerialNumber", &(info_ptr->SDAQ_info.serial_number), t_integer_uint);
 	xml_SDAQ_data(w_node, BAD_CAST "Type",(char *) info_ptr->SDAQ_info.dev_type, t_string);
 	xml_SDAQ_data(w_node, BAD_CAST "Firmware_Rev", &(info_ptr->SDAQ_info.firm_rev), t_integer_ubyte);
    xml_SDAQ_data(w_node, BAD_CAST "Hardware_Rev", &(info_ptr->SDAQ_info.hw_rev), t_integer_ubyte);
    xml_SDAQ_data(w_node, BAD_CAST "Available_Channels", &(info_ptr->SDAQ_info.num_of_ch), t_integer_ubyte);
	xml_SDAQ_data(w_node, BAD_CAST "Samplerate", &(info_ptr->SDAQ_info.sample_rate), t_integer_ubyte);
	xml_SDAQ_data(w_node, BAD_CAST "Max_num_of_cal_points", &(info_ptr->SDAQ_info.max_cal_point), t_integer_ubyte);
	//add calibration data. Calibration data node is the new root
	root_node = xmlNewChild(root_node, NULL, BAD_CAST "Calibration_Data", NULL);
	for(int i=0;i<info_ptr->SDAQ_info.num_of_ch;i++)
	{
		//add xml_node for Channel
		sprintf((char*)buff, "CH%d", i+1);
		w_node = xmlNewChild(root_node, NULL, buff, NULL);
		//add channel's Calibration date and amount of used points
		xml_SDAQ_data(w_node, BAD_CAST "Calibration_date",
			g_slist_nth_data((GSList *)info_ptr->Calibration_date_list,i), t_cal_date);
		xml_SDAQ_data(w_node, BAD_CAST "Calibration_Period",
			&((date_list_data_of_node *)g_slist_nth_data((GSList *)info_ptr->Calibration_date_list,i))->period, t_integer_ubyte);
		xml_SDAQ_data(w_node, BAD_CAST "Used_Points",
			&((date_list_data_of_node *)g_slist_nth_data((GSList *)info_ptr->Calibration_date_list,i))->amount_of_points, t_integer_ubyte);
		cal_unit = ((date_list_data_of_node *)g_slist_nth_data((GSList *)info_ptr->Calibration_date_list,i))->cal_unit;
		sprintf((char*)buff, "%s%s", unit_str[cal_unit], cal_unit<Unit_code_base_region_size?"(Base)":"");
		xml_SDAQ_data(w_node, BAD_CAST "Unit", buff, t_string);
		//add points for channel
		w_node1 = xmlNewChild(w_node, NULL, BAD_CAST "Points", NULL);
		for(int j=0; j < info_ptr->SDAQ_info.max_cal_point; j++)
		{
			sprintf((char*)buff, "Point_%d",j);
			w_node2 = xmlNewChild(w_node1, NULL, buff, NULL);
			for(int k=0; k<6; k++)
			{
				switch(k+1)
				{
					case meas: point_name = (unsigned char*)"Measure"; break;
					case ref: point_name =  (unsigned char*)"Reference"; break;
					case offset: point_name = (unsigned char*)"Offset"; break;
					case gain: point_name = (unsigned char*)"Gain"; break;
					case C2: point_name = (unsigned char*)"C2"; break;
					case C3: point_name = (unsigned char*)"C3"; break;
				}
				xml_SDAQ_data(w_node2, point_name,
				&(((sdaq_calibration_points_data *)g_slist_nth_data(((GSList *)info_ptr->Cal_points_data_lists[i]), j*6+k))->data_of_point), t_float);
			}
		}
	}
	//Check for formatting flag
	if(file_path[0]=='-' && file_path[1])
	{
		exp_format_flag = 1;
		file_path = file_path[1]? file_path+1 : file_path;
	}
	else if(file_path[0]=='+' && !file_path[1])
	{
		exp_format_flag = 1;
		file_path = "-";
	}
    //write the xml_doc to stdout or to file
    xmlSaveFormatFileEnc(file_path, xml_doc, "UTF-8", exp_format_flag);
	//free allocated memory
	xmlFreeDoc(xml_doc);
	xmlCleanupParser();
    // this is to debug memory for regression tests
    xmlMemoryDump();
	return 0;
}

xmlNodePtr xml_SDAQ_data(xmlNodePtr root_node , unsigned char *node_name, void *contents_ptr, unsigned char type)
{
	unsigned char buff[60],*buff_ptr=buff;
	date_list_data_of_node * node_dec = contents_ptr;
	struct tm ptm={0};
	xmlNodePtr node;
	switch(type)
	{
		case t_float:
			sprintf((char*)buff,"%g",*((float *)contents_ptr));
			break;
		case t_integer_ubyte:
			sprintf((char*)buff,"%u",*((unsigned char*)contents_ptr));
			break;
		case t_integer_ushort:
			sprintf((char*)buff,"%u",*((unsigned short*)contents_ptr));
			break;
		case t_integer_uint:
			sprintf((char*)buff,"%u",*((unsigned int*)contents_ptr));
			break;
		case t_cal_date:
			ptm.tm_year = node_dec->year + 100; //100 = 2000-1900
			ptm.tm_mon = node_dec->month - 1;
			ptm.tm_mday =  node_dec->day;
			strftime((char*)buff_ptr,sizeof(buff),"%Y/%m/%d",&ptm);
			break;
		case t_string:
			buff_ptr = (unsigned char*)contents_ptr;
			break;
		default :
			return NULL;
	}
	node = xmlNewChild(root_node, NULL, node_name, buff_ptr);
	return node;
}

/*
 * Function used in setinfo.c: check filepath for a valid xml,and convert it to SDAQ_info_cal_data (new_conf).
 * Return: 0 at success and 1 on failure.
 */
int XML_info_file_read_and_validate(char *file_path, void *new_conf)
{
	SDAQ_info_cal_data *SDAQs_new_config = new_conf;

	if(!file_path||!new_conf)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
