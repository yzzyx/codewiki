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
#include <errno.h>
#include <time.h>
#include <fcgi_stdio.h>
#include "codewiki.h"

struct config {
	char *wiki_url;
	char *content_path;
};
struct config c;

struct cgi_var {
	LIST_ENTRY(cgi_var)	entry;
	char			*name;
	char			*value;
};
LIST_HEAD(cgi_var_list, cgi_var);
struct cgi_var_list cgi_vars;
struct cgi_var_list cookie_vars;

#undef printf
void
cgi_urldecode(char *data)
{
	char		*ptr;
	char		hex[3];
	int		i, ch, len;

	hex[2] = '\0';

	if (data == NULL)
		return;
	printf("decoding string %s\n", data);

	len = strlen(data);
	for (i = 0; i < len; i++) {
		if (data[i] == '%') {
			/* check if we've reached the end */
			if (i + 2 > len)
				return;

			printf("data: %.3s\n", data +i);
			ptr = data + i + 1;
			hex[0] = *ptr++;
			hex[1] = *ptr++;

			printf("hex: %s\n", hex);

			errno = 0;
			ch = (char)strtol(hex, NULL, 16);
			if (errno) /* Something fishy? */
				continue;

			data[i] = ch;

			printf("Pre_move: %s\n", data);

			/* Move data in our array (including \0) */
			memmove(data+i+1, data+i+3, len - i - 2);
			printf("post_move: %s\n", data);
		} else if (data[i] == '+') {
			data[i] = ' ';
		}
	}
}

static
void cgi_split_str(char *str, struct cgi_var_list *list)
{
	char		*ptr;
	char		*end_ptr;
	char		*next_ptr;
	struct cgi_var	*cv;

	ptr = str;
	while (ptr != NULL) {
		if (*ptr == '\0')
			break;

		cv = malloc(sizeof *cv);
		end_ptr = strchr(ptr, '=');
		next_ptr = strchr(ptr, '&');

		printf("ptrs: %p, %p, %p\n", ptr, end_ptr, next_ptr);

		if (end_ptr == NULL ||
		    (next_ptr != NULL && end_ptr > next_ptr)) {
			cv->name = NULL;
			if (next_ptr == NULL)
				cv->value = strdup(ptr);
			else {
				cv->value = strndup(ptr, next_ptr - ptr);
				next_ptr ++;
			}
		} else if (next_ptr != NULL) {
			cv->name = strndup(ptr, end_ptr - ptr);
			cv->value = strndup(end_ptr+1, next_ptr - end_ptr - 1);
			next_ptr ++;
		} else {
			cv->name = strndup(ptr, end_ptr - ptr);
			cv->value = strdup(end_ptr+1);
		}

		if (cv->name)
			cgi_urldecode(cv->name);
		if (cv->value)
			cgi_urldecode(cv->value);

		printf("name: %s\n", cv->name);
		printf("value: %s\n", cv->value);

		LIST_INSERT_HEAD(list, cv, entry);
		ptr = next_ptr;
	}
}

int
cgi_set_vars(char *GET, char *POST, char *COOKIE)
{

	printf("GET: %s\n", GET);
	cgi_split_str(GET, &cgi_vars);
	printf("POST: %s\n", POST);
	cgi_split_str(POST, &cgi_vars);
	printf("COOKIE:\n");
	cgi_split_str(COOKIE, &cookie_vars);
	return (0);
}
#define printf FCGI_printf

int
cgi_clear_vars()
{
	struct cgi_var		*cv;

	while(cgi_vars.lh_first != NULL) {
		cv = cgi_vars.lh_first;
		LIST_REMOVE(cgi_vars.lh_first, entry);

		if (cv->name)
			free(cv->name);
		if (cv->value)
			free(cv->value);
		free(cv);
	}

	while(cookie_vars.lh_first != NULL) {
		cv = cookie_vars.lh_first;
		LIST_REMOVE(cookie_vars.lh_first, entry);
		if (cv->name)
			free(cv->name);
		if (cv->value)
			free(cv->value);
		free(cv);
	}

	return (0);
}

static char *
cgi_get_type(struct cgi_var_list *lst, const char *variable_name)
{
	struct cgi_var	*cv;

	LIST_FOREACH(cv, lst, entry) {
		if (cv->name == NULL) {
			if (variable_name == NULL)
				return (cv->value);
			continue;
		}
		if (strcmp(cv->name, variable_name) == 0)
			return (cv->value);
	}

	return NULL;
}

#define cgi_get(v) cgi_get_type(&cgi_vars, v)
#define cgi_get_cookie(v) cgi_get_type(&cookie_vars, v)

static int
cgi_get_int(const char *variable_name)
{
	char		*val_str;
	int		val;

	val_str = cgi_get(variable_name);
	if (val_str == NULL)
		return -1;

	val = strtol(val_str, NULL, 10);
	return val;
}

static void
cgi_set_cookie(const char *cookie_name, const char *value)
{
	printf("Set-Cookie: %s=%s\n\r", cookie_name, value);
}


int
webserver_output(char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vprintf(fmt,ap);
	va_end(ap);

	return ret;
}

static int
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
	char		*err_str;
	char		*requested_page;
	char		*ptr;
	int		nb, ret;
	char		*ticket;

	char		*GET, *POST, *COOKIES;
	int		POST_len;

	GET = NULL;
	COOKIES = NULL;
	POST = NULL;
	POST_len = -1;

	c.wiki_url = "/wiki";

	LIST_INIT(&cgi_vars);
	LIST_INIT(&cookie_vars);
	page_init();

	while ((ret = FCGI_Accept()) >= 0) {

		/* Initialize variables */
		nb = 0;
		err_str = NULL;

		/* Is this a POST? */
		ptr = getenv("CONTENT_LENGTH");
		if (ptr != NULL)
			nb = strtol(ptr, NULL, 10);

		if (nb > 0) {
			nb ++; /* Space for \0 */
			if (nb > POST_len) {
				if ((POST = realloc(POST,nb)) == NULL) {
					POST_len = -1;
					goto err;
				}
				POST_len = nb;
			}

			if ((ptr = fgets(POST, nb + 1, stdin)) == NULL)
				goto err;
		} else if (POST)
			POST[0] = '\0';


		GET = getenv("QUERY_STRING");
		cgi_set_vars(GET, POST, COOKIES);

		requested_page = cgi_get("p");

		if (cgi_get_int("login") > 0) {
			char *user, *password;

			user = cgi_get("user");
			password = cgi_get("password");

			ret = wiki_login(user, password);
			if (ret == WIKI_LOGIN_WRONG_PASSWORD) {
				err_str = "No such user/password!";
				goto err;
			}

			if (ret == WIKI_LOGIN_ERROR) {
				err_str = "Unknown error - try again later.";
				goto err;
			}

			ticket = wiki_ticket_get(user);
			free(ticket);
//			cgi_set_cookie("wiki_ticket", ticket);
		} else if (cgi_get_int("logout") > 0) {
			ticket = cgi_get_cookie("wiki_ticket");
			wiki_ticket_clear(ticket);
			cgi_set_cookie("wiki_ticket", NULL);

			free(ticket);
		}

		if (cgi_get_int("save") > 0) {
			int	access;

			ticket = cgi_get_cookie("wiki_ticket");
			access = wiki_ticket_access(ticket, requested_page);

			if (!(access & WIKI_TICKET_WRITE)) {
				err_str = "You don't have enough rights to update this page!";
				goto err;
			}

			wiki_save_data(requested_page, cgi_get("wikiText"));
		}

		//cgi_GET(query_string, "p");

		/* FIXME - check if X-SendFile can be used */
		if (requested_page &&
		    (strncmp(requested_page, "css/", strlen("css/")) == 0 ||
		    strncmp(requested_page, "js/", strlen("js/")) == 0 ||
		    strncmp(requested_page, "static/", strlen("static/")))
		    == 0) {

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
		printf("QUERY_STRING: %s<br>\n", getenv("QUERY_STRING"));
		printf("CONTENT_LENGTH: %d<br>\n", nb);

		if (POST)
			printf("POST:%s<br>\n", POST);

		struct cgi_var	*cv;
		LIST_FOREACH(cv, &cgi_vars, entry) {
			if (cv->name) printf("%s", cv->name);
			printf(":");
			if (cv->value) printf("%s", cv->value);
			printf("<br>\n");
		}

		if (cgi_get_int("edit") > 0)
			page_serve(requested_page, 1);
		else
			page_serve(requested_page, 0);
		FCGI_Finish();
		cgi_clear_vars();

		/* Cleanup */
		page_clear();

		continue;
err:
		printf("Content-Type: text/html\r\n\r\n");
		fprintf(stdout,"Error processing request\n");

		FCGI_Finish();
		page_clear();
	}
	fflush(stdout);

	return 0;
}
