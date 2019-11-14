/*   
Copyright (C) 12019-12020  Sam harry Tzavaras

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
	t_time_t,
	t_string
};

//custom function that convert an type (contens_type) to a node with name name_mode
xmlNodePtr xml_SDAQ_data(xmlNodePtr root_node , unsigned char *node_name, void *contents_ptr, unsigned char type);


int XML_info_file_write(char *file_path, void *arg)
{
	SDAQ_info_cal_data *info_ptr = arg;
	xmlDocPtr doc = NULL;
    xmlNodePtr root_node = NULL, w_node = NULL,  w_node1 = NULL, w_node2 = NULL;
	unsigned char buff[15],*buff_ptr;
    //Creates a new document, a node and set it as a root node
    doc = xmlNewDoc(BAD_CAST "1.0");
    root_node = xmlNewNode(NULL, BAD_CAST "SDAQ");
	xmlDocSetRootElement(doc, root_node);
	//add SDAQ info to xml
	w_node = xmlNewChild(root_node, NULL, BAD_CAST "SDAQ_info", NULL);
	xml_SDAQ_data(w_node, BAD_CAST "SerialNumber", &(info_ptr->SDAQ_info.serial_number), t_integer_uint);
 	xml_SDAQ_data(w_node, BAD_CAST "Type",(char *) info_ptr->SDAQ_info.dev_type, t_string);
 	xml_SDAQ_data(w_node, BAD_CAST "Firmware_Rev", &(info_ptr->SDAQ_info.firm_rev), t_integer_ubyte);
    xml_SDAQ_data(w_node, BAD_CAST "Hardware_Rev", &(info_ptr->SDAQ_info.hw_rev), t_integer_ubyte);
    xml_SDAQ_data(w_node, BAD_CAST "Available_Channels", &(info_ptr->SDAQ_info.num_of_ch), t_integer_ubyte);
	xml_SDAQ_data(w_node, BAD_CAST "Samplerate", &(info_ptr->SDAQ_info.sample_rate), t_integer_ubyte);
	//add calibration data. Calibration data node is the new root
	root_node = xmlNewChild(root_node, NULL, BAD_CAST "Calibration_Data", NULL);
	for(int i=0;i<info_ptr->SDAQ_info.num_of_ch;i++)
	{
		//add xml_node for Channel 
		sprintf((char*)buff, "CH%d", i+1);
		w_node = xmlNewChild(root_node, NULL, buff, NULL);
		//add channel's expiration date and amount of used points
		xml_SDAQ_data(w_node, BAD_CAST "Expiration_Date",
			&((date_list_data_of_node *)g_slist_nth_data((GSList *)info_ptr->Calibration_date_list,i))->date, t_time_t);
		xml_SDAQ_data(w_node, BAD_CAST "Used_Points",
			&((date_list_data_of_node *)g_slist_nth_data((GSList *)info_ptr->Calibration_date_list,i))->amount_of_points, t_integer_ubyte);
		//add points for channel
		w_node1 = xmlNewChild(w_node, NULL, BAD_CAST "Points", NULL);
		for(int j=0; j<8; j++)
		{
			sprintf((char*)buff, "Point_%d",j);
			w_node2 = xmlNewChild(w_node1, NULL, buff, NULL);
			for(int k=0; k<2; k++)
			{
				buff_ptr = !k ? (unsigned char*)"Measure" :  (unsigned char*)"Reference";
				xml_SDAQ_data(w_node2, buff_ptr,
				&(((sdaq_calibration_points_data *)g_slist_nth_data(((GSList *)info_ptr->Cal_points_data_lists[i]),j*2+k))->data_of_point), t_float);
			}
		}
	}
    //Dumping document to stdio or file
    xmlSaveFormatFileEnc(file_path, doc, "UTF-8", file_path[0]!='-');
	//free allocated memory
	xmlFreeDoc(doc);
	xmlCleanupParser();
    // this is to debug memory for regression tests
    xmlMemoryDump();
	return 0;
}

xmlNodePtr xml_SDAQ_data(xmlNodePtr root_node , unsigned char *node_name, void *contents_ptr, unsigned char type)
{
	unsigned char buff[60],*buff_ptr=buff;
	struct tm * ptm;
	xmlNodePtr node;
	switch(type)
	{
		case t_float:
			sprintf((char*)buff,"%f",*((float *)contents_ptr));
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
		case t_time_t:
			ptm = gmtime(((time_t*)contents_ptr));
			strftime((char*)buff_ptr,sizeof(buff),"%Y/%m",ptm);
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
