/*
File: ver.c, Implementation of functions related to release info.
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
#include <time.h>

static char buf[20];

char* get_release_date(void)
{
#ifdef RELEASE_DATE
	time_t release_date = RELEASE_DATE;
	strftime(buf, sizeof(buf), "%x - %I:%M%p", localtime(&release_date));
	return buf;
#endif
	return "NO RELEASE_DATE";
}
char* get_compile_date(void)
{
#ifdef COMPILE_DATE
	time_t compile_date = COMPILE_DATE;
	strftime(buf, sizeof(buf), "%x - %I:%M%p", localtime(&compile_date));
	return buf;
#else
	return "NO COMPILE_DATE";
#endif
}
char* get_curr_git_hash(void)
{
#ifdef RELEASE_HASH
	return RELEASE_HASH;
#else
	return "NO RELEASE_HASH";
#endif
}
