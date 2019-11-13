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

#include "Modes.h"
#include "SDAQ_xml.h"

enum contens_type{
	t_float,
	t_integer,
	t_string
};


int add_xml_node(xmlNodePtr root_node , unsigned char node_name, void *contents_ptr, unsigned char type);


int XML_info_file_write(char *file_path, void *arg)
{
	SDAQ_info_cal_data *info_ptr = arg;
	printf("The file Saving function is not yet implemented\n\tThe bellow is garbage\n ");
	
	xmlDocPtr doc = NULL;//document pointer
    xmlNodePtr root_node = NULL, node = NULL, node1 = NULL;// node pointers
    char buff[256];
    int i, j;

    //Creates a new document, a node and set it as a root node
    doc = xmlNewDoc(BAD_CAST "1.0");
    root_node = xmlNewNode(NULL, BAD_CAST "SDAQ");
    xmlDocSetRootElement(doc, root_node);

 	
 	xmlNewChild(root_node, NULL, BAD_CAST "node1", BAD_CAST "content of node 1");
    
    
    
    
    // Creates a DTD declaration. Isn't mandatory.
    //xmlCreateIntSubset(doc, BAD_CAST "root", NULL, BAD_CAST "tree2.dtd");

    //xmlNewChild() creates a new node, which is "attached" as child node of root_node node. 
    xmlNewChild(root_node, NULL, BAD_CAST "node1", BAD_CAST "content of node 1");

    /* 
     * xmlNewProp() creates attributes, which is "attached" to an node.
     * It returns xmlAttrPtr, which isn't used here.
     */
    node =
        xmlNewChild(root_node, NULL, BAD_CAST "node3",
                    BAD_CAST "this node has attributes");
    xmlNewProp(node, BAD_CAST "attribute", BAD_CAST "yes");
    xmlNewProp(node, BAD_CAST "foo", BAD_CAST "bar");

    /*
     * Here goes another way to create nodes. xmlNewNode() and xmlNewText
     * creates a node and a text node separately. They are "attached"
     * by xmlAddChild() 
     */
    node = xmlNewNode(NULL, BAD_CAST "node4");
    node1 = xmlNewText(BAD_CAST
                   "other way to create content (which is also a node)");
    xmlAddChild(node, node1);
    xmlAddChild(root_node, node);

    /* 
     * A simple loop that "automates" nodes creation 
     */
    for (i = 5; i < 7; i++) {
        sprintf(buff, "node%d", i);
        node = xmlNewChild(root_node, NULL, BAD_CAST buff, NULL);
        for (j = 1; j < 4; j++) {
            sprintf(buff, "node%d%d", i, j);
            node1 = xmlNewChild(node, NULL, BAD_CAST buff, NULL);
            xmlNewProp(node1, BAD_CAST "odd", BAD_CAST((j % 2) ? "no" : "yes"));
        }
    }

    //Dumping document to stdio or file
    xmlSaveFormatFileEnc(file_path, doc, "UTF-8", file_path[0]!='-');
    //xmlSaveFormatFileEnc("-", doc, "UTF-8", 1);
    //free the document
    xmlFreeDoc(doc);
    //Free the global variables that may have been allocated by the parser.
    xmlCleanupParser();
    // this is to debug memory for regression tests
    xmlMemoryDump();
	return 0;
}
/*
xmlNodePtr n;
    xmlDocPtr doc;
    xmlChar *xmlbuff;
    int buffersize;

    
    //Create the document.
    doc = xmlNewDoc(BAD_CAST "1.0");
    n = xmlNewNode(NULL, BAD_CAST "root");
    xmlNodeSetContent(n, BAD_CAST "content");
    xmlDocSetRootElement(doc, n);

	//Dump the document to a buffer and print it
	//for demonstration purposes.
    xmlDocDumpFormatMemory(doc, &xmlbuff, &buffersize, 1);
    printf("%s", (char *) xmlbuff);

    //Free associated memory.
    xmlFree(xmlbuff);
    xmlFreeDoc(doc);
*/
