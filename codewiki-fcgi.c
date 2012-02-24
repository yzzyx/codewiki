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
#include <stdarg.h>
#include <malloc.h>
#include <time.h>
#include <fcgi_stdio.h>
#include "codewiki.h"

int debug = 10;
void (*debugcb)() = NULL;

struct config {
	char *wiki_url;
	char *content_path;
};
struct config c;

int webserver_output(char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vprintf(fmt,ap);
	va_end(ap);

	return ret;
}

int
strcmpsuffix(char *str, char *suffix)
{
	int l1, l2;

	l1 = strlen(str);
	l2 = strlen(suffix);

	if (l1 < l2) return -1;
	return strncmp(str+l1-l2, suffix, l2);
}

int
main(int argc, char *argv[], char *envp[])
{
	FILE		*fd;
	char		buf[1024];
	char		*ptr;
	char		*requested_page;
	int nb;
	int ret;

	c.wiki_url = "/wiki";

	while ((ret = FCGI_Accept()) >= 0) {

		requested_page = getenv("QUERY_STRING");

	//if (scgi_fetchstring(scgi, "SCRIPT_URL", buf, sizeof(buf))
	    //== SCGI_OK) {
		//if (strncmp(buf, c.wiki_url, strlen(c.wiki_url)) != 0) {
			//printf("Illegal request - check your wiki_url "
			   //"setting\n");
			//return 0;
		//}
		//requested_page = buf + strlen(c.wiki_url);
		//if (*requested_page == '/') requested_page ++;


		/* FIXME - check if X-SendFile can be used */

		if (strncmp(requested_page, "css/", strlen("css/")) == 0 ||
		    strncmp(requested_page, "js/", strlen("js/")) == 0 ||
		    strncmp(requested_page, "static/", strlen("static/")) == 0) {

			if (strcmpsuffix(requested_page, ".css") == 0)
				printf("Content-Type: text/css\r\n\r\n");
			else
				printf("Content-Type: text/plain\r\n\r\n");

			if ((fd = fopen(requested_page, "r")) == NULL) {
				FCGI_Finish();
				continue;
			}
			while (!feof(fd)) {
				nb = fread(buf, 1, sizeof buf, fd);
				if (nb == 0 && ferror(fd))
					break;
				fwrite(buf, 1, nb, stdout);
			}
			fclose(fd);
			/* Handle js */
			FCGI_Finish();
			continue;
		}

		//if ((ptr = strchr(requested_page, '/')))
			//*ptr = '\0';

		printf("Content-Type: text/html\r\n\r\n");
		page_serve(requested_page, 1);
		FCGI_Finish();
		//printf("<html>\n"
		    //"<body>\n");

		//for(nb = 0;envp[nb] != NULL;nb++)
			//printf("%s<br>\n", envp[nb]);
		//printf("QUERY_STRING: %s\n", getenv("QUERY_STRING"));
		//printf("</body>\n"
		    //"</html>\n");
	}
	fflush(stdout);

	return 0;
}
