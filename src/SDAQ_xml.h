/*
File: SDAQ_xml.c, Declaration of functions for read and write SDAQ related XMLs
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

/* SDAQ_xml function declaration*/
/*Function used in getinfo.c: convert the arg (aka SDAQ_info_cal_data*) to xml.
  if file_path is valid save, otherwise it's print it to stdout*/
int XML_info_file_write(char *file_path, void *arg, unsigned char format_flag);

/*
 * Function used in setinfo.c: check filepath for a valid xml,and convert it to SDAQ_info_cal_data.
 * Return: 0 at success and 1 on failure.
 */
int XML_info_file_read_and_validate(char *file_path, void *new_conf);
