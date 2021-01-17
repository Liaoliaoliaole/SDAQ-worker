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
//SDAQ_xml function implementation
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

#include "info.h"//including -> "SDAQ_drv.h", "Modes.h"
#include "SDAQ_xml.h"

enum contens_type{
	t_float,
	t_integer_ubyte,
	t_integer_ushort,
	t_integer_uint,
	t_cal_date, //Special, send wholly container
	t_string
};

//Custom function that convert an type (contens_type) to a node with name name_mode
xmlNodePtr xml_SDAQ_data(xmlNodePtr root_node , unsigned char *node_name, void *contents_ptr, unsigned char type);

int XML_info_file_write(char *file_path, void *arg, unsigned char exp_format_flag)
{
	SDAQ_info_cal_data *info_ptr = arg;
	xmlDocPtr xml_doc = NULL;
    xmlNodePtr root_node = NULL, w_node = NULL,  w_node1 = NULL, w_node2 = NULL;
	unsigned char buff[20], *point_name, cal_unit;
    //Creates a new document, a node and set it as a root node
    xml_doc = xmlNewDoc(BAD_CAST "1.0");
    root_node = xmlNewNode(NULL, BAD_CAST "SDAQ");
	xmlDocSetRootElement(xml_doc, root_node);
	//Add SDAQ info to xml
	w_node = xmlNewChild(root_node, NULL, BAD_CAST "SDAQ_info", NULL);
	xml_SDAQ_data(w_node, BAD_CAST "SerialNumber", &(info_ptr->SDAQ_info.serial_number), t_integer_uint);
 	xml_SDAQ_data(w_node, BAD_CAST "Type",(char *) info_ptr->SDAQ_info.dev_type, t_string);
 	xml_SDAQ_data(w_node, BAD_CAST "Firmware_Rev", &(info_ptr->SDAQ_info.firm_rev), t_integer_ubyte);
    xml_SDAQ_data(w_node, BAD_CAST "Hardware_Rev", &(info_ptr->SDAQ_info.hw_rev), t_integer_ubyte);
    xml_SDAQ_data(w_node, BAD_CAST "Available_Channels", &(info_ptr->SDAQ_info.num_of_ch), t_integer_ubyte);
	xml_SDAQ_data(w_node, BAD_CAST "Samplerate", &(info_ptr->SDAQ_info.sample_rate), t_integer_ubyte);
	xml_SDAQ_data(w_node, BAD_CAST "Max_num_of_cal_points", &(info_ptr->SDAQ_info.max_cal_point), t_integer_ubyte);
	//Add calibration data. Calibration data node is the new root
	root_node = xmlNewChild(root_node, NULL, BAD_CAST "Calibration_Data", NULL);
	for(int i=0;i<info_ptr->SDAQ_info.num_of_ch;i++)
	{
		//Add xml_node for Channel
		sprintf((char*)buff, "CH%d", i+1);
		w_node = xmlNewChild(root_node, NULL, buff, NULL);
		//Add channel's Calibration date and amount of used points
		xml_SDAQ_data(w_node, BAD_CAST "Calibration_date",
			g_slist_nth_data((GSList *)info_ptr->Calibration_date_list,i), t_cal_date);
		xml_SDAQ_data(w_node, BAD_CAST "Calibration_Period",
			&((date_list_data_of_node *)g_slist_nth_data((GSList *)info_ptr->Calibration_date_list,i))->period, t_integer_ubyte);
		xml_SDAQ_data(w_node, BAD_CAST "Used_Points",
			&((date_list_data_of_node *)g_slist_nth_data((GSList *)info_ptr->Calibration_date_list,i))->amount_of_points, t_integer_ubyte);
		cal_unit = ((date_list_data_of_node *)g_slist_nth_data((GSList *)info_ptr->Calibration_date_list,i))->cal_unit;
		//sprintf((char*)buff, "%s%s", unit_str[cal_unit], cal_unit<Unit_code_base_region_size?"(Base)":"");
		sprintf((char*)buff, "%s", unit_str[cal_unit]);
		xml_SDAQ_data(w_node, BAD_CAST "Unit", buff, t_string);
		//Add points for channel
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
    //Write the xml_doc to stdout or to file
    xmlSaveFormatFileEnc(file_path, xml_doc, "UTF-8", exp_format_flag);
	//Free allocated memory
	xmlFreeDoc(xml_doc);
	xmlCleanupParser();
    //This is to debug memory for regression tests
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

		//-- Local XML read/get Functions--//
//Get an node by it's name. Search in the space of root_node. Return node on found, NULL otherwise.
xmlNode * get_XML_node_by_name(xmlNode *root_node, const char *Node_name);
//Populate the SDAQ_info section of the new_config with data from XML. Return EXIT_SUCCESS on success, EXIT_FAILURE otherwise.
int populate_SDAQ_info(xmlNode *SDAQ_info, SDAQ_info_cal_data *SDAQs_new_config);
//Populate Calibration_date_list and Cal_points_data_lists sections of new_config. Return EXIT_SUCCESS on success, otherwise EXIT_FAILURE.
int populate_Calibration_Data(xmlNode *Calibration_Data, SDAQ_info_cal_data *SDAQs_new_config);

int find_appearances_of_a_XML_node(xmlNode *root_node, const char *Node_name)
{
	int cnt=0;
	xmlNode *cur_node;

	if (root_node->type == XML_ELEMENT_NODE)
	{
		for (cur_node = root_node->children; cur_node; cur_node = cur_node->next)
		{
			if (cur_node->type == XML_ELEMENT_NODE)
			{
				if(!strcmp((char *)(cur_node->name), Node_name))
					cnt++;
			}
		}
	}
	return cnt;
}

/*
 * Function used in setinfo.c: check filepath for a valid xml,and convert it to SDAQ_info_cal_data (new_conf).
 * Return: 0 at success and 1 on failure.
 */
int XML_info_file_read_and_validate(char *file_path, void *new_conf)
{
	SDAQ_info_cal_data *SDAQs_new_config = new_conf;
	int SDAQ_info_cnt, Calibration_Data_cnt, retval = EXIT_FAILURE;
	int wc;
	GString *gstring_from_stdin;
	char *filename = file_path;
	xmlDocPtr doc = NULL;
	xmlNode *SDAQ_root = NULL, *SDAQ_info = NULL, *Calibration_Data = NULL;

	if(!file_path||!new_conf)
		return EXIT_FAILURE;
    //--- Parse the file or the data from STDIN ---//
    if(file_path[0]=='-')//Check if the data comes from STDIN
	{
		filename = "STDIN";
		if(!(gstring_from_stdin = g_string_new(NULL)))
		{
			fprintf(stderr, "Memory Error!!!\n");
			exit(EXIT_FAILURE);
		}
		while((wc = getchar()) != EOF)
			gstring_from_stdin = g_string_append_c(gstring_from_stdin, wc);
		doc = xmlReadMemory(gstring_from_stdin->str, gstring_from_stdin->len, "", NULL, XML_PARSE_NOBLANKS);
		g_string_free(gstring_from_stdin, TRUE);
	}
	else
		doc = xmlReadFile(filename, NULL, XML_PARSE_NOBLANKS);
	if(doc)
    {
		/*Get the root element node */
		SDAQ_root = xmlDocGetRootElement(doc);
		if(!strcmp((const char*)(SDAQ_root->name), "SDAQ"))
		{
			SDAQ_info_cnt=find_appearances_of_a_XML_node(SDAQ_root, "SDAQ_info");
			Calibration_Data_cnt=find_appearances_of_a_XML_node(SDAQ_root, "Calibration_Data");
			if(SDAQ_info_cnt==1 && Calibration_Data_cnt==1)
			{
				SDAQ_info=get_XML_node_by_name(SDAQ_root, "SDAQ_info");
				Calibration_Data=get_XML_node_by_name(SDAQ_root, "Calibration_Data");
				if(SDAQ_info && Calibration_Data)
				{
					if(!(retval = populate_SDAQ_info(SDAQ_info, SDAQs_new_config)))
						retval = populate_Calibration_Data(Calibration_Data, SDAQs_new_config);
				}
				else
				{
					if(!SDAQ_info)
						fprintf(stderr, "XML node \"SDAQ_info\" Not found!!!\n");
					if(!Calibration_Data)
						fprintf(stderr, "XML node \"Calibration_Data\" Not found!!!\n");
				}
			}
			else
			{
				if(SDAQ_info_cnt>1)
					fprintf(stderr, "XML node \"SDAQ_info\" found %d times!!!\n", SDAQ_info_cnt);
				if(Calibration_Data_cnt>1)
					fprintf(stderr, "XML node \"Calibration_Data\" found %d times!!!\n", Calibration_Data_cnt);
			}
		}
		else
			fprintf(stderr, "XML's root element name != \"SDAQ\"\n");
	}
	else
		fprintf(stderr, "Failed to parse %s\n", filename);
	//Free allocated memory
	xmlFreeDoc(doc);
	xmlCleanupParser();
    xmlMemoryDump();
	return retval;
}

xmlNode * get_XML_node_by_name(xmlNode *root_node, const char *Node_name)
{
	xmlNode *cur_node;
	if (root_node->type == XML_ELEMENT_NODE)
	{
		for (cur_node = root_node->children; cur_node; cur_node = cur_node->next)
		{
			if (cur_node->type == XML_ELEMENT_NODE)
			{
				if(!strcmp((char *)(cur_node->name), Node_name))
					return cur_node;
			}
		}
	}
	return NULL;
}

xmlChar * _xmlNodeGetContent(xmlChar *content, const xmlNode *cur)
{
	if(content)
		xmlFree(content);
	content = xmlNodeGetContent(cur);
	if(!(*content))
	{
		xmlFree(content);
		content = NULL;
	}
	return content;
}

int populate_SDAQ_info(xmlNode *SDAQ_info, SDAQ_info_cal_data *SDAQs_new_config)
{
	unsigned char i;
	xmlNode *SerialNumber = get_XML_node_by_name(SDAQ_info, "SerialNumber"),
			*Type = get_XML_node_by_name(SDAQ_info, "Type"),
			*Firmware_Rev = get_XML_node_by_name(SDAQ_info, "Firmware_Rev"),
			*Hardware_Rev = get_XML_node_by_name(SDAQ_info, "Hardware_Rev"),
			*Available_Channels = get_XML_node_by_name(SDAQ_info, "Available_Channels"),
			*Samplerate = get_XML_node_by_name(SDAQ_info, "Samplerate"),
			*Max_num_of_cal_points = get_XML_node_by_name(SDAQ_info, "Max_num_of_cal_points");
	xmlChar *content = NULL;
	if(SerialNumber&&Type&&Firmware_Rev&&Hardware_Rev&&Available_Channels&&Samplerate&&Max_num_of_cal_points)
	{
		if(*(content = _xmlNodeGetContent(content, SerialNumber)))
			SDAQs_new_config->SDAQ_info.serial_number = atoi((const char *)(content));
		else
		{
			fprintf(stderr, "XML node SDAQ_info->SerialNumber does not have content!!!\n");
			return EXIT_FAILURE;
		}
		if((content = _xmlNodeGetContent(content, Type)))
		{
			for(i=0;i<Unit_code_base_region_size;i++)
			{
				if(!dev_type_str[i]||!strcmp((const char *)content, dev_type_str[i]))
					break;
			}
			if(!dev_type_str[i])
			{
				fprintf(stderr, "Unknown type of SDAQ (%s)!!!\n",content);
				xmlFree(content);
				return EXIT_FAILURE;
			}
			SDAQs_new_config->SDAQ_info.dev_type = dev_type_str[i];
		}
		else
		{
			fprintf(stderr, "XML node SDAQ_info->Type does not have content!!!\n");
			return EXIT_FAILURE;
		}
		if((content = _xmlNodeGetContent(content, Firmware_Rev)))
			SDAQs_new_config->SDAQ_info.firm_rev = atoi((const char *)(content));
		else
		{
			fprintf(stderr, "XML node SDAQ_info->Firmware_Rev does not have content!!!\n");
			return EXIT_FAILURE;
		}
		if((content = _xmlNodeGetContent(content, Hardware_Rev)))
			SDAQs_new_config->SDAQ_info.hw_rev = atoi((const char *)(content));
		else
		{
			fprintf(stderr, "XML node SDAQ_info->Hardware_Rev does not have content!!!\n");
			return EXIT_FAILURE;
		}
		if((content = _xmlNodeGetContent(content, Available_Channels)))
			SDAQs_new_config->SDAQ_info.num_of_ch = atoi((const char *)(content));
		else
		{
			fprintf(stderr, "XML node SDAQ_info->Available_Channels does not have content!!!\n");
			return EXIT_FAILURE;
		}
		if((content = _xmlNodeGetContent(content, Samplerate)))
			SDAQs_new_config->SDAQ_info.sample_rate = atoi((const char *)(content));
		else
		{
			fprintf(stderr, "XML node SDAQ_info->Samplerate does not have content!!!\n");
			return EXIT_FAILURE;
		}
		if((content = _xmlNodeGetContent(content, Max_num_of_cal_points)))
			SDAQs_new_config->SDAQ_info.max_cal_point = atoi((const char *)(content));
		else
		{
			fprintf(stderr, "XML node SDAQ_info->Max_num_of_cal_points does not have content!!!\n");
			return EXIT_FAILURE;
		}
		xmlFree(content);
	}
	else
	{
		if(!SerialNumber)
			fprintf(stderr, "XML node SDAQ_info->SerialNumber Not found!!!\n");
		if(!Type)
			fprintf(stderr, "XML node SDAQ_info->Type Not found!!!\n");
		if(!Firmware_Rev)
			fprintf(stderr, "XML node SDAQ_info->Firmware_Rev Not found!!!\n");
		if(!Hardware_Rev)
			fprintf(stderr, "XML node SDAQ_info->Hardware_Rev Not found!!!\n");
		if(!Available_Channels)
			fprintf(stderr, "XML node SDAQ_info->Available_Channels Not found!!!\n");
		if(!Samplerate)
			fprintf(stderr, "XML node SDAQ_info->Samplerate Not found!!!\n");
		if(!Max_num_of_cal_points)
			fprintf(stderr, "XML node SDAQ_info->Max_num_of_cal_points Not found!!!\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

GSList * Cal_points_data_list_conv_and_append(GSList *Cal_points_for_channel_list, float val_of_point, unsigned char point_type, unsigned char point_num)
{
	sdaq_calibration_points_data *new_point_node; //sdaq_calibration_points_data data for search and work pointer for GSList;

	if(!(new_point_node = new_SDAQ_cal_point_node()))
	{
		fprintf(stderr, "Memory Error!!!\n");
		exit(EXIT_FAILURE);
	}
	new_point_node->data_of_point = val_of_point;
	new_point_node->type = point_type;
	new_point_node->points_num = point_num;
	Cal_points_for_channel_list = g_slist_append(Cal_points_for_channel_list, new_point_node);
	return Cal_points_for_channel_list;
}

int populate_Calibration_Data(xmlNode *Calibration_Data, SDAQ_info_cal_data *SDAQs_new_config)
{
	char point_name_buff[10];
	unsigned char channel, amount_of_new_cal_ch;
	int i, j;
	date_list_data_of_node l_new_date_note, *new_date_node; //date_list_data_of_node local_variable and work pointer for GSList;
	xmlChar *content = NULL;
	xmlNode *XML_channel_root = NULL,
			*XML_cal_date = NULL,
			*XML_period = NULL,
			*XML_used_Points = NULL,
			*XML_CH_Unit = NULL,
			*XML_points_data = NULL, *XML_point_data = NULL;
	//XML Nodes for Point data.
	xmlNode *XML_Meas = NULL,
			*XML_Ref = NULL,
			*XML_Offset = NULL,
			*XML_Gain = NULL,
			*XML_C2 = NULL,
			*XML_C3 = NULL;

	if(!Calibration_Data || !SDAQs_new_config)
		return EXIT_FAILURE;
	if(!SDAQs_new_config->SDAQ_info.num_of_ch)
	{
		fprintf(stderr, "SDAQ_info.num_of_ch is ZERO!!!\n");
		return EXIT_FAILURE;
	}
	if(!(SDAQs_new_config->Cal_points_data_lists = calloc(SDAQs_new_config->SDAQ_info.num_of_ch, sizeof(struct GSList *))))
	{
		fprintf(stderr, "Memory Error!!!\n");
		exit(EXIT_FAILURE);
	}
	amount_of_new_cal_ch = xmlChildElementCount(Calibration_Data);
	if(amount_of_new_cal_ch>SDAQs_new_config->SDAQ_info.num_of_ch)
		amount_of_new_cal_ch = SDAQs_new_config->SDAQ_info.num_of_ch;
	for(i=0, XML_channel_root=Calibration_Data->children; i<amount_of_new_cal_ch; XML_channel_root=XML_channel_root->next,i++)
	{
		sscanf((char *)(XML_channel_root->name),"CH%hhu",&channel);
		if(!channel || channel>SDAQs_new_config->SDAQ_info.num_of_ch)
		{
			if(!channel)
				fprintf(stderr, "Name of Calibration_Data->%s is invalid!!!\n",XML_channel_root->name);
			if(channel>SDAQs_new_config->SDAQ_info.num_of_ch)
				fprintf(stderr, "Name of Calibration_Data->%s is Out of range (0<CHn<=%d)!!!\n", XML_channel_root->name, SDAQs_new_config->SDAQ_info.num_of_ch);
			return EXIT_FAILURE;
		}
		if(g_slist_find_custom((GSList *)(SDAQs_new_config->Calibration_date_list), &channel, SDAQ_date_node_with_channel_b_find))//Check if channel is already registered.
			continue;
		XML_cal_date = get_XML_node_by_name(XML_channel_root, "Calibration_date");
		XML_period = get_XML_node_by_name(XML_channel_root, "Calibration_Period");
		XML_used_Points = get_XML_node_by_name(XML_channel_root, "Used_Points");
		XML_CH_Unit = get_XML_node_by_name(XML_channel_root, "Unit");
		XML_points_data = get_XML_node_by_name(XML_channel_root, "Points");
		if(XML_cal_date && XML_period && XML_used_Points && XML_CH_Unit && XML_points_data)
		{
			l_new_date_note.ch_num = channel;
			if((content = _xmlNodeGetContent(content, XML_cal_date)))
			{
				short year,month,day;
				sscanf((char*)content,"%hd/%hd/%hd",&year,&month,&day);
				if((year>=2000&&year<=2255)&&(month>=1&&month<=12)&&(day>=1&&day<=31))
				{
					l_new_date_note.year = year-2000;
					l_new_date_note.month = month;
					l_new_date_note.day = day;
				}
				else
				{
					fprintf(stderr, "Content of XML node SDAQ_info.CH%d->Calibration_date(%d/%d/%d) is wrong!!!\n",channel,year,month,day);
					xmlFree(content);
					return EXIT_FAILURE;
				}
			}
			else
			{
				fprintf(stderr, "XML node SDAQ_info.CH%d->Calibration_date does not have content!!!\n", channel);
				return EXIT_FAILURE;
			}
			if((content = _xmlNodeGetContent(content, XML_period)))
				l_new_date_note.period = atoi((char*)content);
			else
			{
				fprintf(stderr, "XML node SDAQ_info.CH%d->Calibration_Period does not have content!!!\n", channel);
				return EXIT_FAILURE;
			}
			if((content = _xmlNodeGetContent(content, XML_used_Points)))
			{
				l_new_date_note.amount_of_points = atoi((char*)content);
				if(l_new_date_note.amount_of_points>SDAQs_new_config->SDAQ_info.max_cal_point)
				{
					fprintf(stderr, "XML node SDAQ_info.CH%d->Used_Points > SDAQs_new_config->SDAQ_info.max_cal_point!!!\n", channel);
					xmlFree(content);
					return EXIT_FAILURE;
				}
			}
			else
			{
				fprintf(stderr, "XML node SDAQ_info.CH%d->Used_Points does not have content!!!\n", channel);
				return EXIT_FAILURE;
			}
			if((content = _xmlNodeGetContent(content, XML_CH_Unit)))
			{
				for(j=Unit_code_base_region_size;j<256;j++)
				{
					if(!unit_str[j]||!strcmp((char*)content, unit_str[j]))
						break;
				}
				if(unit_str[j])
					l_new_date_note.cal_unit = j;
				else
				{
					fprintf(stderr, "Unit(%s) in content of XML node SDAQ_info.CH%d->Unit is unknown!!!\n",content,channel);
					xmlFree(content);
					return EXIT_FAILURE;
				}
			}
			else
			{
				fprintf(stderr, "XML node SDAQ_info.CH%d->XML_CH_Unit does not have content!!!\n", channel);
				return EXIT_FAILURE;
			}
			xmlFree(content); content = NULL;
			//Append data to GSList: SDAQs_new_config->Calibration_date_list;
			if(!(new_date_node = new_SDAQ_date_node()))
			{
				fprintf(stderr, "Memory Error!!!\n");
				exit(EXIT_FAILURE);
			}
			memcpy(new_date_node, &l_new_date_note, sizeof(date_list_data_of_node));
			SDAQs_new_config->Calibration_date_list = (struct GSList *)g_slist_append((GSList *)SDAQs_new_config->Calibration_date_list, new_date_node);
			//Check if calibration point is set and if yes load them at SDAQs_new_config->Cal_points_data_lists[ch].
			if(l_new_date_note.amount_of_points)
			{
				for(j=0, XML_point_data=XML_points_data->children; j<l_new_date_note.amount_of_points; XML_point_data=XML_point_data->next,j++)
				{
					sprintf(point_name_buff, "Point_%hhu",j);
					if(!XML_point_data || strcmp((char*)(XML_point_data->name), point_name_buff))
					{
						fprintf(stderr, "XML node for calibration point %d for channel %d was not found or it's in wrong order!!!\n", j, channel);
						return EXIT_FAILURE;
					}
					XML_Meas = get_XML_node_by_name(XML_point_data, "Measure");
					XML_Ref = get_XML_node_by_name(XML_point_data, "Reference");
					XML_Offset = get_XML_node_by_name(XML_point_data, "Offset");
					XML_Gain = get_XML_node_by_name(XML_point_data, "Gain");
					XML_C2 = get_XML_node_by_name(XML_point_data, "C2");
					XML_C3 = get_XML_node_by_name(XML_point_data, "C3");
					if(XML_Meas && XML_Ref && XML_Offset && XML_Gain && XML_C2 && XML_C3)
					{
						if((content = _xmlNodeGetContent(content, XML_Meas)))
							SDAQs_new_config->Cal_points_data_lists[channel-1] = (struct GSList *)Cal_points_data_list_conv_and_append((GSList *)SDAQs_new_config->Cal_points_data_lists[channel-1], atof((char*)content), meas, j);
						else
						{
							fprintf(stderr, "XML node Calibration_Data->CH%d->Points->Point_%d->Measure does not have content!!!\n", channel, j);
							return EXIT_FAILURE;
						}
						if((content = _xmlNodeGetContent(content, XML_Ref)))
							SDAQs_new_config->Cal_points_data_lists[channel-1] = (struct GSList *)Cal_points_data_list_conv_and_append((GSList *)SDAQs_new_config->Cal_points_data_lists[channel-1], atof((char*)content), ref, j);
						else
						{
							fprintf(stderr, "XML node Calibration_Data->CH%d->Points->Point_%d->Reference does not have content!!!\n", channel, j);
							return EXIT_FAILURE;
						}
						if((content = _xmlNodeGetContent(content, XML_Offset)))
							SDAQs_new_config->Cal_points_data_lists[channel-1] = (struct GSList *)Cal_points_data_list_conv_and_append((GSList *)SDAQs_new_config->Cal_points_data_lists[channel-1], atof((char*)content), offset, j);
						else
						{
							fprintf(stderr, "XML node Calibration_Data->CH%d->Points->Point_%d->Offset does not have content!!!\n", channel, j);
							return EXIT_FAILURE;
						}
						if((content = _xmlNodeGetContent(content, XML_Gain)))
							SDAQs_new_config->Cal_points_data_lists[channel-1] = (struct GSList *)Cal_points_data_list_conv_and_append((GSList *)SDAQs_new_config->Cal_points_data_lists[channel-1], atof((char*)content), gain, j);
						else
						{
							fprintf(stderr, "XML node Calibration_Data->CH%d->Points->Point_%d->Gain does not have content!!!\n", channel, j);
							return EXIT_FAILURE;
						}
						if((content = _xmlNodeGetContent(content, XML_C2)))
							SDAQs_new_config->Cal_points_data_lists[channel-1] = (struct GSList *)Cal_points_data_list_conv_and_append((GSList *)SDAQs_new_config->Cal_points_data_lists[channel-1], atof((char*)content), C2, j);
						else
						{
							fprintf(stderr, "XML node Calibration_Data->CH%d->Points->Point_%d->C2 does not have content!!!\n", channel, j);
							return EXIT_FAILURE;
						}
						if((content = _xmlNodeGetContent(content, XML_C3)))
							SDAQs_new_config->Cal_points_data_lists[channel-1] = (struct GSList *)Cal_points_data_list_conv_and_append((GSList *)SDAQs_new_config->Cal_points_data_lists[channel-1], atof((char*)content), C3, j);
						else
						{
							fprintf(stderr, "XML node Calibration_Data->CH%d->Points->Point_%d->C3 does not have content!!!\n", channel, j);
							return EXIT_FAILURE;
						}
						xmlFree(content); content = NULL;
					}
					else
					{
						if(!XML_Meas)
							fprintf(stderr, "XML node Calibration_Data->CH%d->Points->Point_%d->Measure Not found!!!\n", channel, j);
						if(!XML_Ref)
							fprintf(stderr, "XML node Calibration_Data->CH%d->Points->Point_%d->Reference Not found!!!\n", channel, j);
						if(!XML_Offset)
							fprintf(stderr, "XML node Calibration_Data->CH%d->Points->Point_%d->Offset Not found!!!\n", channel, j);
						if(!XML_Gain)
							fprintf(stderr, "XML node Calibration_Data->CH%d->Points->Point_%d->Gain Not found!!!\n", channel, j);
						if(!XML_C2)
							fprintf(stderr, "XML node Calibration_Data->CH%d->Points->Point_%d->C2 Not found!!!\n", channel, j);
						if(!XML_C3)
							fprintf(stderr, "XML node Calibration_Data->CH%d->Points->Point_%d->C3 Not found!!!\n", channel, j);
						return EXIT_FAILURE;
					}
				}
			}
			else
				SDAQs_new_config->Cal_points_data_lists[channel-1] = NULL;
		}
		else
		{
			if(!XML_cal_date)
				fprintf(stderr, "XML node SDAQ_info.CH%d->Calibration_date Not found!!!\n", channel);
			if(!XML_period)
				fprintf(stderr, "XML node SDAQ_info.CH%d->Calibration_Period Not found!!!\n", channel);
			if(!XML_used_Points)
				fprintf(stderr, "XML node SDAQ_info.CH%d->Used_Points Not found!!!\n", channel);
			if(!XML_CH_Unit)
				fprintf(stderr, "XML node SDAQ_info.CH%d->Unit Not found!!!\n", channel);
			if(!XML_points_data)
				fprintf(stderr, "XML node SDAQ_info.CH%d->Points Not found!!!\n", channel);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
