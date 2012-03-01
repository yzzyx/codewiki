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
#include "codewiki.h"

struct config config;

char *header = NULL;
char *footer = NULL;

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
	webserver_output(
"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \n"
"\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
"<html>\n"
"  <head>\n"
"    <title>%s</title>\n"
"    <meta content=\"text/html; charset=UTF-8\" "
"http-equiv=\"Content-Type\" />\n", r->requested_page);

	TAILQ_FOREACH(pp, &r->stylesheets, entry) {
		webserver_output("<link href=\"%s\" media=\"all\" rel=\"stylesheet\" "
		    "type=\"text/css\"/>\n", pp->str);
	}

	TAILQ_FOREACH(pp, &r->scripts, entry) {
		webserver_output("<script type=\"text/javascript\" src=\"%s\">"
		    "</script>\n", pp->str);
	}

	webserver_output("  </head>\n"
	    "<body onload=\"sh_highlightDocument();\">\n"
	    "<div id=\"body\">\n");

	if (header != NULL)
		webserver_output("<div id=\"header\">%s</div>\n", header);

	webserver_output("  <div id=\"contents\">\n");
	TAILQ_FOREACH(pp, &r->page_contents, entry) {
		webserver_output("%s", pp->str);
	}
	webserver_output("  </div>\n");
	if (footer != NULL)
		webserver_output("<div id=\"footer\">%s</div>\n", footer);

	webserver_output("</div>\n"
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
	webserver_output(
"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \n"
"\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
"<html>\n"
"  <head>\n"
"    <title>Edit page %s</title>\n"
"    <meta content=\"text/html; charset=UTF-8\" "
"http-equiv=\"Content-Type\" />\n", page_name);

	webserver_output("<link href=\"/static/admin.css\" media=\"all\" "
	    "rel=\"stylesheet\" type=\"text/css\"/>\n");

	webserver_output("<script type=\"text/javascript\" src=\"/static/js/admin.js\">"
		    "</script>\n");

	webserver_output("  </head>\n"
	    "<body>\n"
	    "<div id=\"body\">\n");

	if (header != NULL)
		webserver_output("<div id=\"header\">%s</div>\n", header);

	webserver_output("<form method=\"POST\" action=\"?\">\n"
	    "<input type=\"hidden\" name=\"p\" value=\"%s\" />\n"
	    "<input type=\"hidden\" name=\"time\" value=\"%ld\" />\n"
	    "<input type=\"hidden\" name=\"save\" value=\"1\" />\n",
	    page_name, time(NULL));
	webserver_output("<div id=\"contents\">\n"
	    "<h2>Editing page %s</h2>\n"
	    "<textarea id=\"wikiText\" name=\"wikiText\" tabindex=\"1\">\n",
	    page_name);

	if (page_data != NULL)
		webserver_output("%s", page_data);

	webserver_output("</textarea>");

	webserver_output("<input type=\"submit\" value=\"Save\" />\n"
	    "</div>\n"
	    "</form>\n");

	if (footer != NULL)
		webserver_output("<div id=\"footer\">%s</div>\n", footer);

	webserver_output("</div>\n"
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

	return (0);
}

int
wiki_request_serve(struct wiki_request *r)
{
	FILE		*fd;
	char		*page;
	int		st, nb;
	char		buf[1024];

	/* Default page */
	if (r->requested_page == NULL)
		r->requested_page = "_main";

	/* Handle static content - also, css and images must be
	 * handled separately
	 */
	if (strncmp(r->requested_page, "static/", strlen("static/")) == 0 ||
	    (r->edit == 0 &&
	     (strncmp(r->requested_page, "img/", strlen("img/")) == 0 ||
	     strncmp(r->requested_page, "css/", strlen("css/")) == 0))) {
	    
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


		/* FIXME - images can have different MIME-types,
		 * this should be sent to the webserver.
		 */
		// webserver_set_mime_type("text/css");
		// webserver_set_mime_type("text/plain");
		// webserver_set_mime_type("text/html");
		// webserver_set_mime_type("image/png");
		// webserver_set_mime_type("image/gif");
		// webserver_set_mime_type("image/jpg");
		if ((fd = fopen(r->requested_page, "r")) == NULL) {
			return (-1);
		}

		while (!feof(fd)) {
			nb = fread(buf, 1, sizeof buf, fd);
			if (nb == 0 && ferror(fd))
				break;
			webserver_output_buf(buf, nb);
		}
		fclose(fd);
		return (0);
	}

	if (r->edit == 0) {
		/* check if any parts of the page has been updated or
		 * need generating
		 */
		st = wiki_stat_page(r->requested_page);
		if (st == STAT_PAGE_NO_UPDATES) {
			DPRINTF("no update, send generated\n");
			page = wiki_load_generated(r->requested_page);
			webserver_output("%s", page);
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
		page = wiki_load_data("_header");
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
		page = wiki_load_data("_footer");
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
	page = wiki_load_data(r->requested_page);
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

int
wiki_login(const char *username, const char *password)
{
	/* FIXME - check wiki-passwdfile */
	return WIKI_LOGIN_OK;
}

char *
wiki_ticket_get(const char *username)
{
	/* FIXME - get value from wiki-passwdfile */
	return strdup("ABCDEFGHIJKLMN=)SASAFJKSF");
}

int
wiki_ticket_access(const char *ticket, const char *page)
{
	return WIKI_TICKET_READ | WIKI_TICKET_WRITE;
}

int
wiki_ticket_clear(const char *ticket)
{
	return (0);
}
