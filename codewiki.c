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
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <time.h>
#include "codewiki.h"

#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#define TICKET_SIZE 20
#define TICKET_VALID_CHARS "abcdefghijklmnopqrstuvwxyz" \
			   "ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
			   ".:-_()[]"

struct ticket {
	char			*ticket;
	char			*user;
	int			access;
	RB_ENTRY(ticket)	entry;
};
RB_HEAD(ticket_list, ticket);
RB_PROTOTYPE(ticket_list, ticket, entry, ticket_rb_cmp);

struct ticket_list tickets;

int
ticket_rb_cmp(struct ticket *a, struct ticket *b)
{
	return strcmp(a->ticket, b->ticket);
}
RB_GENERATE(ticket_list, ticket, entry, ticket_rb_cmp);

struct config config;

char *header = NULL;
char *footer = NULL;

int
strcmpsuffix(const char *str, const char *suffix)
{
	int l1, l2;

	if (str == NULL)
		return (-1);

	l1 = strlen(str);
	l2 = strlen(suffix);

	if (l1 < l2)
		return (-1);
	return strncmp(str+l1-l2, suffix, l2);
}

int
stylesheet_add(struct wiki_request *r, char *file)
{
	struct page_part	*pp;

	/* FIXME - sanitize input */
	TAILQ_FOREACH(pp, &r->stylesheets, entry) {
		if (strcmp(pp->str, file) == 0)
			return (0);
	}

	pp = malloc(sizeof *pp);
	pp->str = file;
	TAILQ_INSERT_TAIL(&r->stylesheets, pp, entry);
	return (0);
}

int
script_add(struct wiki_request *r, char *file)
{
	struct page_part	*pp;

	/* FIXME - sanitize input */
	TAILQ_FOREACH(pp, &r->scripts, entry) {
		if (strcmp(pp->str, file) == 0)
			return (0);
	}

	pp = malloc(sizeof *pp);
	pp->str = file;
	TAILQ_INSERT_TAIL(&r->scripts, pp, entry);
	return (0);
}

static int
page_insert(struct wiki_request *r, char *s)
{
	struct page_part	*pp;

	pp = malloc(sizeof *pp);
	pp->str = s;

	TAILQ_INSERT_TAIL(&r->page_contents, pp, entry);

	return (0);
}

static int
page_parse(struct wiki_request *r, char *from, char *to)
{
	char		*ptr, *prev_ptr;
	struct tag	*m;

	if (from == NULL)
		return (0);

	if (to)
		*to = '\0';
	prev_ptr = from;
	for (ptr = from; *ptr && ptr != to; ptr++) {
		m = find_tag(r, ptr);
		if (m) {

			/* push previous data and start-tag to head*/
			*ptr = '\0';
			if (prev_ptr < ptr)
				page_insert(r, prev_ptr);
			page_insert(r, m->start_tag);

			/* parse everything inbetween (if anything) */
			if (m->end_ptr) {
				if (m->parse)
					page_parse(r, m->start_ptr, m->end_ptr);
				else {
					*m->end_ptr = '\0';
					page_insert(r, m->start_ptr);
				}
			}

			/* push end-tag */
			if (m->end_tag)
				page_insert(r, m->end_tag);

			prev_ptr = m->skip_ptr;
			ptr = m->skip_ptr - 1;
		}
	}
	page_insert(r, prev_ptr);
	return (0);
}

static char *
page_get(struct wiki_request *r)
{
	struct page_part	*pp;
	char			*buf, *ptr;
	int			len = 0;

	/* Calculate size of buffer */
	TAILQ_FOREACH(pp, &r->page_contents, entry) {
		len += strlen(pp->str);
	}

	if (len == 0)
		return NULL;

	/* FIXME - handle errors */
	buf = malloc(len+1);

	/* Copy all data to buffer */
	ptr = buf;
	TAILQ_FOREACH(pp, &r->page_contents, entry) {
		strcpy(ptr, pp->str);
		ptr += strlen(ptr);
	}
	*ptr = '\0';
	return buf;
}

static int
page_print(struct wiki_request *r)
{
	struct page_part	*pp;

	/* Headers */
	webserver_output(r,
"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \n"
"\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
"<html>\n"
"  <head>\n"
"    <title>%s</title>\n"
"    <meta content=\"text/html; charset=UTF-8\" "
"http-equiv=\"Content-Type\" />\n", r->requested_page);

	TAILQ_FOREACH(pp, &r->stylesheets, entry) {
		webserver_output(r, "<link href=\"%s\" media=\"all\" rel=\"stylesheet\" "
		    "type=\"text/css\"/>\n", pp->str);
	}

	TAILQ_FOREACH(pp, &r->scripts, entry) {
		webserver_output(r,
		    "<script type=\"text/javascript\" src=\"%s\">"
		    "</script>\n", pp->str);
	}

	webserver_output(r, "  </head>\n"
	    "<body onload=\"sh_highlightDocument();\">\n"
	    "<div id=\"body\">\n");

	if (header != NULL)
		webserver_output(r, "<div id=\"header\">%s</div>\n", header);

	webserver_output(r, "  <div id=\"contents\">\n");
	TAILQ_FOREACH(pp, &r->page_contents, entry) {
		webserver_output(r, "%s", pp->str);
	}
	webserver_output(r, "  </div>\n");
	if (footer != NULL)
		webserver_output(r, "<div id=\"footer\">%s</div>\n", footer);

	webserver_output(r, "</div>\n"
	    "</body>\n"
	    "</html>\n");

	return (0);
}

static int
page_edit(struct wiki_request *r, char *page_data)
{
	char		*page_name;

	page_name = r->requested_page;
	if (page_name == NULL)
		page_name = "_main";

	/* Generate edit-interface */

	/* Headers */
	webserver_output(r,
"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \n"
"\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
"<html>\n"
"  <head>\n"
"    <title>Edit page %s</title>\n"
"    <meta content=\"text/html; charset=UTF-8\" "
"http-equiv=\"Content-Type\" />\n", page_name);

	webserver_output(r,
	    "<link href=\"/static/css/admin.css\" media=\"all\" "
	    "rel=\"stylesheet\" type=\"text/css\"/>\n");

	webserver_output(r,
	    "<script type=\"text/javascript\" src=\"/static/js/admin.js\">"
		    "</script>\n");

	webserver_output(r,
	    "  </head>\n"
	    "<body>\n"
	    "<div id=\"body\">\n");

	if (header != NULL)
		webserver_output(r, "<div id=\"header\">%s</div>\n", header);

	webserver_output(r, "<form method=\"POST\" action=\"?\">\n"
	    "<input type=\"hidden\" name=\"p\" value=\"%s\" />\n"
	    "<input type=\"hidden\" name=\"time\" value=\"%ld\" />\n"
	    "<input type=\"hidden\" name=\"save\" value=\"1\" />\n",
	    page_name, time(NULL));

	if (r->page_type == WIKI_PAGE_TYPE_IMAGE) {
		webserver_output(r, "<div id=\"contents\">\n"
			"<h2>Editing image %s</h2>\n"
			"Current image:<br />\n"
			"<img id=\"wikiImage\" src=\"%s/%s\" /><br />"
			"New image:<br />\n"
			"<input type=\"file\" name=\"wikiData\"><br />",
			page_name, config.base_url, r->requested_page);
	} else { /* HTML and CSS */
		webserver_output(r, "<div id=\"contents\">\n"
		    "<h2>Editing page %s</h2>\n"
		    "<textarea id=\"wikiText\" name=\"wikiData\" "
				"tabindex=\"1\">\n", page_name);

		if (page_data != NULL)
			webserver_output(r, "%s", page_data);

		webserver_output(r, "</textarea>");
	}

	webserver_output(r, "<input type=\"submit\" value=\"Save\" />\n"
	    "</div>\n"
	    "</form>\n");

	if (footer != NULL)
		webserver_output(r, "<div id=\"footer\">%s</div>\n", footer);

	webserver_output(r, "</div>\n"
	    "</body>\n"
	    "</html>\n");

	return (0);

}

struct wiki_request *
wiki_request_new()
{
	struct wiki_request	*r;

	if ((r = malloc(sizeof *r)) == NULL)
		return (NULL);

	TAILQ_INIT(&r->page_contents);
	TAILQ_INIT(&r->stylesheets);
	TAILQ_INIT(&r->scripts);

	r->requested_page = NULL;
	r->mime_type = NULL;
	r->edit = 0;
	r->data = NULL;

	return (r);
}

static int
wiki_request_clear_data(struct wiki_request *r)
{
	struct page_part	*pp;

	while(r->page_contents.tqh_first != NULL) {
		pp = r->page_contents.tqh_first;
		TAILQ_REMOVE(&r->page_contents, pp, entry);
		free(pp);
	}

	while(r->stylesheets.tqh_first != NULL) {
		pp = r->stylesheets.tqh_first;
		TAILQ_REMOVE(&r->stylesheets, pp, entry);
		free(pp);
	}

	while(r->scripts.tqh_first != NULL) {
		pp = r->scripts.tqh_first;
		TAILQ_REMOVE(&r->scripts, pp, entry);
		free(pp);
	}

	return (0);
}

int
wiki_request_clear(struct wiki_request *r)
{
	wiki_request_clear_data(r);

	r->requested_page = NULL;
	r->mime_type = NULL;
	r->edit = 0;
	r->data = NULL;

	return (0);
}

int
wiki_init()
{
	init_tags();
	RB_INIT(&tickets);

	return (0);
}

static void
wiki_set_mime_type(struct wiki_request *r)
{

	if (r->page_type == WIKI_PAGE_TYPE_HTML)
		r->mime_type = "text/html";
	else if (r->page_type == WIKI_PAGE_TYPE_CSS)
		r->mime_type = "text/css";
	else if (r->page_type == WIKI_PAGE_TYPE_JS)
		r->mime_type = "text/javascript";
	else if (strcmpsuffix(r->requested_page, ".png") == 0)
		r->mime_type = "image/png";
	else if (strcmpsuffix(r->requested_page, ".gif") == 0)
		r->mime_type = "image/gif";
	else if (strcmpsuffix(r->requested_page, ".jpg") == 0)
		r->mime_type = "image/jpeg";
	else if (strcmpsuffix(r->requested_page, ".jpeg") == 0)
		r->mime_type = "image/jpeg";
	/* The values below actually only apply to static pages */
	else if (strcmpsuffix(r->requested_page, ".txt") == 0)
		r->mime_type = "text/plain";
	else if (strcmpsuffix(r->requested_page, ".html") == 0)
		r->mime_type = "text/html";
	else if (strcmpsuffix(r->requested_page, ".css") == 0)
		r->mime_type = "text/css";
	else if (strcmpsuffix(r->requested_page, ".js") == 0)
		r->mime_type = "text/javascript";
	else
		r->mime_type = "text/html";
}

static void
wiki_set_page_type(struct wiki_request *r)
{

	if (r->requested_page == NULL)
		r->page_type = WIKI_PAGE_TYPE_HTML;
	else if(strncmp(r->requested_page, "css/", strlen("css/")) == 0)
		r->page_type = WIKI_PAGE_TYPE_CSS;
	else if(strncmp(r->requested_page, "js/", strlen("js/")) == 0)
		r->page_type = WIKI_PAGE_TYPE_JS;
	else if(strncmp(r->requested_page, "img/", strlen("img/")) == 0)
		r->page_type = WIKI_PAGE_TYPE_IMAGE;
	else if(strncmp(r->requested_page, "static/", strlen("static/")) == 0)
		r->page_type = WIKI_PAGE_TYPE_STATIC;
	else
		r->page_type = WIKI_PAGE_TYPE_HTML;
}

int
wiki_request_serve(struct wiki_request *r)
{
	FILE		*fd;
	char		*page;
	int		st, nb;
	char		buf[1024];

	DPRINTF("requested_page: %s\n", r->requested_page);

	/* Default page */
	if (r->requested_page == NULL)
		r->requested_page = "_main";

	/* Handle static content - also, css and images must be
	 * handled separately
	 */
	wiki_set_page_type(r);
	if (r->edit) {
		r->mime_type = "text/html";
	} else {
		wiki_set_mime_type(r);
	}

	DPRINTF("page_type: %d\n", r->page_type);
	if (r->page_type == WIKI_PAGE_TYPE_STATIC) {
		/* If we can, we should just pass the filename
		 * to the webserver
		 *
		 * Also, all paths should, if possible point
		 * to a URL that is handled directly by the
		 * webserver - this is just here for cases
		 * where the webserver doesn't/can't handle it
		 * for some reason
		 */
		/* FIXME - this is not implemented */
		//		webserver_output_file(requested_page);
		if ((fd = fopen(r->requested_page, "r")) == NULL) {
			return (-1);
		}

		while (!feof(fd)) {
			nb = fread(buf, 1, sizeof buf, fd);
			if (nb == 0 && ferror(fd))
				break;
			webserver_output_buf(r, buf, nb);
		}
		fclose(fd);
		return (0);
	}

	st = STAT_PAGE_NO_UPDATES;
	if (r->edit == 0) {
		/* Images and CSS should be sent right away */
		if (r->page_type == WIKI_PAGE_TYPE_IMAGE ||
		    r->page_type == WIKI_PAGE_TYPE_CSS) {
			/* FIXME - use X-Sendfile if possible */
			nb = wiki_load_data(r->requested_page, &page);
			webserver_output_buf(r, page, nb);
			free(page);

			return (0);
		}

		/* check if any parts of the page has been updated or
		 * need generating
		 */
		st = wiki_stat_page(r->requested_page);
		if (st == STAT_PAGE_NO_UPDATES) {
			DPRINTF("no update, send generated\n");
			page = wiki_load_generated(r->requested_page);
			webserver_output(r, "%s", page);
			free(page);
			return (0);
		}
	}

	header = NULL;
	footer = NULL;

	if (st & STAT_PAGE_UPDATED_HEADER) {
		/* Generate header */
		/* FIXME - use some kind of lock on
		 * generated.html
		 */
		wiki_load_data("_header", &page);
		if (page) {
			page_parse(r, page, NULL);
			header = page_get(r);
			wiki_save_generated("_header", header);
			wiki_request_clear_data(r);
			free(page);
		}
	} else
		header = wiki_load_generated("_header");

	if (st & STAT_PAGE_UPDATED_FOOTER) {
		/* Generate footer */
		wiki_load_data("_footer", &page);
		if (page) {
			page_parse(r, page, NULL);
			footer = page_get(r);
			wiki_save_generated("_footer", footer);
			wiki_request_clear_data(r);
			free(page);
		}
	} else
		footer = wiki_load_generated("_footer");

	/*strdup_sprintf("%s/css/main.css", static_url);*/
	stylesheet_add(r, "/static/css/style.css");

	DPRINTF("loading page %s\n", r->requested_page);

	if (r->page_type != WIKI_PAGE_TYPE_IMAGE)
		wiki_load_data(r->requested_page, &page);

	if (r->edit) {
		DPRINTF("edit page %s\n", r->requested_page);
		page_edit(r, page);
	} else {
		DPRINTF("parsing page %s\n", r->requested_page);
		page_parse(r, page, NULL);
		page_print(r);
	}
	free(page);

	if (header)
		free(header);
	if (footer)
		free(footer);
	return (0);
}

static int
wiki_load_config_fd(FILE *fd)
{
	/* fparseln() ... */
	return (0);
}

int
wiki_load_config()
{
	FILE		*fd;

	char		*filename_list[] =
	    { "./codewiki.conf",
	      "/etc/codewiki.conf",
	      NULL
	    };
	char		*filename, *ptr;
	int		cnt;

	/* Try the following:
	 * $CODEWIKI_CONFIG
	 * ./codewiki.conf
	 * /etc/codewiki.conf
	 * or use defaults.
	 */
	cnt = 0;
	filename = NULL;
	if ((ptr = getenv("CODEWIKI_CONFIG")) != NULL) {
		filename = ptr;
	}

	do {
		if (filename == NULL)
			filename = filename_list[cnt++];

		if ((fd = fopen(filename, "r")) != NULL) {
			wiki_load_config_fd(fd);
			fclose(fd);
			break;
		}

		filename = filename_list[cnt++];
	} while (filename != NULL);

	if (filename == NULL) {
		config.static_url = "/static";
		config.base_url = "/wiki";
	}

	return (0);
}

static int
wiki_ticket_set(const char *username, int access)
{
	struct ticket	*new;
	char		*key;
	int		i;

	/* FIXME - check if we already have it */
	new = malloc(sizeof *new);
	key = malloc(TICKET_SIZE+1);
	for(i=0;i<TICKET_SIZE;i++)
		key[i] = TICKET_VALID_CHARS[rand() %
			     sizeof(TICKET_VALID_CHARS)];
	key[i] = '\0';

	new->ticket = key;
	new->user = strdup(username);
	new->access = access;
	DPRINTF("Adding key '%s' for user %s\n", new->ticket, new->user);

	RB_INSERT(ticket_list, &tickets, new);

	return (0);
}

int
wiki_login(const char *username, const char *password)
{
	FILE		*fd;
	char		*buf;
	char		*ptr, *saveptr;
	int		ret;

	char		*p_usr,
			*p_passwd;
	char		*enc_passwd;

	if ((buf = malloc(50)) == NULL)
		return (WIKI_LOGIN_ERROR);

	if ((fd = fopen("passwd","r")) == NULL) {
		free(buf);
		return (WIKI_LOGIN_WRONG_PASSWORD);
	}

	ret = WIKI_LOGIN_WRONG_PASSWORD;
	while (!feof(fd)) {
		fgets(buf, 50, fd);
		ptr = buf;
		while (isspace(*ptr)) ptr++;

		if (*ptr == '#')
			continue;

		DPRINTF("passwd entry: %s", ptr);

		saveptr = NULL;
		p_usr = strtok_r(ptr, ":", &saveptr);
		if (strcmp(p_usr, username) != 0)
			continue;
		p_passwd = strtok_r(NULL, ":", &saveptr);

		DPRINTF("passwd password: %s\n", p_passwd);
		/*
		p_write = strtok_r(NULL, ":", &saveptr);
		p_wspec = strtok_r(NULL, ":", &saveptr);
		*/

		if (p_passwd == NULL)
			continue;

		enc_passwd = crypt(password, p_passwd);
		DPRINTF("supplied: %s\n", enc_passwd);

		if (strcmp(enc_passwd, p_passwd) != 0)
			continue;

		DPRINTF("Login ok!\n");
		wiki_ticket_set(username, WIKI_TICKET_READ |
		    WIKI_TICKET_WRITE);

		ret = WIKI_LOGIN_OK;
		break;
	}

	free(buf);
	return (ret);
}

char *
wiki_ticket_get(const char *username)
{
	struct ticket	*t;

	RB_FOREACH(t, ticket_list, &tickets) {
		if (strcmp(t->user, username) == 0)
			return t->ticket;
	}

	return (NULL);
}

int
wiki_ticket_access(const char *ticket, const char *page)
{
	struct ticket	search;
	struct ticket	*t;

	if (ticket == NULL)
		return (WIKI_TICKET_READ);

	search.ticket = (char *)ticket;
	t = RB_FIND(ticket_list, &tickets, &search);

	if (!t)
		return (WIKI_TICKET_READ);

	DPRINTF("User '%s' has ticket - access %d\n", t->user, t->access);
	return (t->access);
}

int
wiki_ticket_clear(const char *ticket)
{
	return (0);
}
