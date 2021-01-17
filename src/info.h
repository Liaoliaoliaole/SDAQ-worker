/*
File: Info.h, Declaration of functions Shared between setinfo and getinfo.
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
#include <glib.h>
#include <gmodule.h>
#include "SDAQ_drv.h"
#include "Modes.h"

	/*--- Options for corr_SDAQ_info_and_calibration_data() ---*/
#define INFO   (1<<0)
#define DATE   (1<<1)
#define POINTS (1<<2)

	/*----- local functions  -----*/
//Function that request and receive calibration data (info, dates, points) from SDAQ with address: dev_addr. Return: 0 on success or 1 on failure.
int get_SDAQ_info_and_calibration_data(int socket_num, unsigned char dev_addr, unsigned int scanning_time, SDAQ_info_cal_data *SDAQ_cal_config);
//Function that request and receive Device info and calibration dates from a SDAQ with address: dev_addr. Return: 0 on success or 1 on failure.
int get_SDAQ_info(int socket_num, unsigned char dev_addr, unsigned int scanning_time, SDAQ_info_cal_data *SDAQ_cal_config);
//Function that request and receive calibration dates and points from a SDAQ with with address: dev_addr. Return: 0 on success or 1 on failure.
/*
 * CH_Req is a **, structure like SDAQ_info_cal_data.Cal_points_data_lists. Used to determinate which channel's point will requested.
 * If CH_Req == NULL, points from all channels will be request.
 */
int get_SDAQ_calibration_data(int socket_num, unsigned char dev_addr, unsigned int scanning_time, SDAQ_info_cal_data *SDAQ_cal_config, void **CH_Req);

//Function that send the data from SDAQ_info_cal_data *str to SDAQ with address: dev_addr. Return: 0 on success or 1 on failure.
int set_SDAQ_info_and_calibration_data(int socket_num, unsigned char dev_addr, SDAQ_info_cal_data *SDAQ_cal_config);

//Function that correlate two SDAQ_info_cal_data (new and current config). Return EXIT_SUCCESS, if new related with the old; otherwise EXIT_FAILURE.
int corr_SDAQ_info_and_calibration_data(SDAQ_info_cal_data *cur_conf, SDAQ_info_cal_data *new_conf, unsigned char options);

//Declaration of function for Calibration_date_list
date_list_data_of_node* new_SDAQ_date_node();//allocate memory for a new sdaq_calibration_date.
void free_SDAQ_Date_node(gpointer Date_node);//used with g_slist_free_full to free the data of node.
gint SDAQ_point_node_with_type_and_num_find(gconstpointer a, gconstpointer b);//GFunc function used with g_slist_find_custom.
gint SDAQ_date_node_with_nonzero_amount_of_points_find(gconstpointer a, gconstpointer b);//GFunc function used with g_slist_find_custom.
gint SDAQ_date_node_with_channel_b_find(gconstpointer a, gconstpointer b);//GFunc type function used with g_slist_find_custom.

//Declaration of function for Cal_points_data_lists
sdaq_calibration_points_data* new_SDAQ_cal_point_node();//allocate memory for a new sdaq_calibration_points_data part of Cal_points_data_lists.
void free_SDAQ_cal_point_node(gpointer Point_node);//used with g_slist_free_full to free the data of node.
//Called from g_slist_foreach. the pass_arg is the array with with the list of calibration data points.
void printf_SDAQ_Date_with_points_node(gpointer Date_node, gpointer pass_arg);

//Function that freeing data of SDAQ_info_cal_data.
void free_SDAQ_info_cal_data(SDAQ_info_cal_data *conf);
