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

extern char **environ;

struct cgi_var {
	LIST_ENTRY(cgi_var)	entry;
	char			*name;
	char			*value;
};
LIST_HEAD(cgi_var_list, cgi_var);
struct cgi_var_list cgi_vars;
struct cgi_var_list cookie_vars;

int		sent_headers;

void
cgi_urldecode(char *data)
{
	char		*ptr;
	char		hex[3];
	int		i, ch, len;

	hex[2] = '\0';

	if (data == NULL)
		return;

	len = strlen(data);
	for (i = 0; i < len; i++) {
		if (data[i] == '%') {
			/* check if we've reached the end */
			if (i + 2 > len)
				return;

			ptr = data + i + 1;
			hex[0] = *ptr++;
			hex[1] = *ptr++;

			errno = 0;
			ch = (char)strtol(hex, NULL, 16);
			if (errno) /* Something fishy? */
				continue;

			data[i] = ch;

			/* Move data in our array (including \0) */
			memmove(data+i+1, data+i+3, len - i - 2);
		} else if (data[i] == '+') {
			data[i] = ' ';
		}
	}
}

static
void cgi_split_str(char *str, struct cgi_var_list *list, char str_sep)
{
	char		*ptr;
	char		*end_ptr;
	char		*next_ptr;
	struct cgi_var	*cv;

	ptr = str;
	while (ptr != NULL) {
		while (*ptr == ' ' || *ptr == '\t') ptr++;

		if (*ptr == '\0')
			break;

		cv = malloc(sizeof *cv);
		end_ptr = strchr(ptr, '=');
		next_ptr = strchr(ptr, str_sep);

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

		LIST_INSERT_HEAD(list, cv, entry);
		ptr = next_ptr;
	}
}

int
cgi_set_vars(char *GET, char *POST, char *COOKIE)
{
	cgi_split_str(GET, &cgi_vars, '&');
	cgi_split_str(POST, &cgi_vars, '&');
	cgi_split_str(COOKIE, &cookie_vars, ';');
	return (0);
}

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
	struct cgi_var	*cv;

	cv = malloc(sizeof *cv);
	cv->name = strdup(cookie_name);
	cv->value = strdup(value);

	LIST_INSERT_HEAD(&cookie_vars, cv, entry);
}

int
webserver_output(struct wiki_request *r, const char *fmt, ...)
{
	struct cgi_var	*cv;
	va_list		ap;
	int		ret;

	if (sent_headers == 0) {
		printf("Content-Type: %s\r\n", r->mime_type);

		LIST_FOREACH(cv, &cookie_vars, entry) {
			if (cv->name == NULL)
				continue;
			printf("Set-Cookie: %s=%s; Path=/\r\n",
			    cv->name, cv->value);
		}
		printf("\r\n");
		sent_headers = 1;
	}
	va_start(ap, fmt);
	ret = vprintf(fmt,ap);
	va_end(ap);

	return (ret);
}

int
webserver_output_buf(struct wiki_request *r, const char *buf, int nb)
{
	struct cgi_var	*cv;

	if (sent_headers == 0) {
		printf("Content-Type: %s\r\n", r->mime_type);

		LIST_FOREACH(cv, &cookie_vars, entry) {
			if (cv->name == NULL)
				continue;
			printf("Set-Cookie: %s=%s; Path=/\r\n",
			    cv->name, cv->value);
		}
		printf("\r\n");
		sent_headers = 1;
	}

	return fwrite((char *)buf, 1, nb, stdout);
}

int
main(int argc, char *argv[], char *envp[])
{
	struct wiki_request	*r;
	char			*err_str;
	char			*ptr;
	int			nb, ret;
	char			*ticket;
	int			page_access;

	char			*POST;
	int			POST_len;

	POST = NULL;
	POST_len = -1;

	LIST_INIT(&cgi_vars);
	LIST_INIT(&cookie_vars);

	wiki_load_config();
	wiki_init();

	r = wiki_request_new();

	while ((ret = FCGI_Accept()) >= 0) {

		/* Initialize variables */
		nb = 0;
		err_str = NULL;
		sent_headers = 0;

		r->requested_page = getenv("PATH_INFO");
		if (r->requested_page == NULL) {
			err_str = "No PATH_INFO supplied. "
			    "Is your webserver configured correctly?";
			goto err;
		}

		r->requested_page += strlen(config.base_url);
		if (*r->requested_page == '/')
			r->requested_page++;

		/* Is this a POST? */
		ptr = getenv("CONTENT_LENGTH");
		if (ptr != NULL)
			nb = strtol(ptr, NULL, 10);

		if (nb > 0) {
			if (nb > POST_len) {
				if ((POST = realloc(POST,nb+1)) == NULL) {
					POST_len = -1;
					goto err;
				}
				POST_len = nb;
			}

			if (fread(POST, 1, nb, stdin)  < nb) {
				err_str = "Could not read request!";
				goto err;
			}
			POST[nb] = '\0';
		} else if (POST)
			POST[0] = '\0';


		cgi_set_vars(getenv("QUERY_STRING"), POST,
		             getenv("HTTP_COOKIE"));

		if (cgi_get_int("login") > 0) {
			char *user, *password;

			user = cgi_get("user");
			password = cgi_get("password");

			if (user == NULL) {
				/* Show login-page */
				r->requested_page = "_login";
				goto show_page;
			}

			ret = wiki_login(user, password);
			if (ret == WIKI_LOGIN_WRONG_PASSWORD)
				err_str = "No such user/password!";
			else if (ret == WIKI_LOGIN_ERROR)
				err_str = "Unknown error - try again later.";

			if (err_str) {
				r->requested_page = "_login";
				goto show_page;
			}

			ticket = wiki_ticket_get(user);
			cgi_set_cookie("wiki_ticket", ticket);
		} else if (cgi_get_int("logout") > 0) {
			ticket = cgi_get_cookie("wiki_ticket");
			wiki_ticket_clear(ticket);
			cgi_set_cookie("wiki_ticket", NULL);
		}

		ticket = cgi_get_cookie("wiki_ticket");
		page_access = wiki_ticket_access(ticket, r->requested_page);

		if (cgi_get_int("save") > 0) {
			if (!(page_access & WIKI_TICKET_WRITE)) {
				err_str = "You don't have enough rights to update this page!";
				goto show_page;
			}

			wiki_save_data(r->requested_page, cgi_get("wikiData"));
		}
		
		if (cgi_get_int("edit") > 0) {
			if (page_access & WIKI_TICKET_WRITE)
				r->edit = 1;
			else {
				err_str = "You don't have enough rights to update this page!";
				r->requested_page = "_login";
			}
		}

		/*
		if (strcmpsuffix(r->requested_page, ".css") == 0)
			webserver_set_mime_type("text/css");
		else if (strcmpsuffix(r->requested_page, ".js") == 0)
			webserver_set_mime_type("text/plain");
		else
			webserver_set_mime_type("text/html");
			*/

show_page:
		wiki_request_serve(r);
		printf("QUERY_STRING: %s<br>\n", getenv("QUERY_STRING"));
		printf("PATH_INFO: %s<br>\n", getenv("PATH_INFO"));
		printf("CONTENT_LENGTH: %d<br>\n", nb);
		if (ticket)
			printf("ticket: %s<br>\n", ticket);

		if (err_str)
			printf("err_str: %s<br>\n", err_str);

		if (POST)
			printf("POST:%s<br>\n", POST);

		struct cgi_var	*cv;
		int i;
		for (i = 0; environ[i] != NULL; i++)
			printf("%s<br>\n", environ[i]);

		LIST_FOREACH(cv, &cookie_vars, entry) {
			if (cv->name) printf("%s", cv->name);
			printf(":");
			if (cv->value) printf("%s", cv->value);
			printf("<br>\n");
		}


		FCGI_Finish();
		cgi_clear_vars();

		/* Cleanup */
		wiki_request_clear(r);

		continue;
err:
		printf("Content-Type: text/html\r\n\r\n");
		printf("Error processing request<br>\n");
		if (err_str)
			printf("%s<br>\n", err_str);

		FCGI_Finish();

		cgi_clear_vars();
		wiki_request_clear(r);
	}
	fflush(stdout);

	return 0;
}
