/*
 codewiki - small wiki-page parser
 Copyright (C) 2012 Elias Norberg

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <scgi.h>
#include "codewiki.h"

struct config {
	char *wiki_url;
	char *content_path;
};
struct config c;

int debug = 10;
void (*debugcb)() = NULL;

int
main(int argc, char *argv[])
{
	char		buf[1024];
	char		*ptr;
	scgi_connection *scgi;
	int ret;

	c.wiki_url = "/wiki";
	scgi = scgi_create_default(NULL, NULL, NULL);
	while ((ret = scgi_input(scgi)) == SCGI_NOTAVAIL);
	if (ret != SCGI_OK) {
		fprintf(stderr, "There was an error during scgi_input()!\n");
		return -1;
	}

	printf("Content-Type: text/html\r\n\r\n");
	if (scgi_fetchstring(scgi, "SCRIPT_URL", buf, sizeof(buf))
	    == SCGI_OK) {
		if (strncmp(buf, c.wiki_url, strlen(c.wiki_url)) != 0) {
			printf("Illegal request - check your wiki_url "
			   "setting\n");
			return 0;
		}
		ptr = buf + strlen(c.wiki_url);
		if (*ptr == '/') ptr ++;
		if ((ptr = strchr(ptr, '/')))
			*ptr = '\0';
		printf("requested page = %s<br />\n",
		    buf + strlen(c.wiki_url));
	}
	if (scgi_fetchstring(scgi, "SCGI", buf, sizeof(buf)) == SCGI_OK)
		printf("SCGI version: %s<br />\n", buf);
	if (scgi_fetchstring(scgi, "test", buf, sizeof(buf)) == SCGI_OK)
		printf("test value: %s<br />\n", buf);
	if (scgi_fetchstring(scgi, "edit", buf, sizeof(buf)) == SCGI_OK)
		printf("edit!<br />\n");
	fflush(stdout);
	scgi_free(scgi);

	return 0;
}
