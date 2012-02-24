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

struct page_part_list page_contents;
struct page_part_list stylesheets;
struct page_part_list scripts;

char *header = NULL;
char *footer = NULL;

int
stylesheet_add(char *file)
{
	struct page_part	*pp;

	/* FIXME - sanitize input */
	TAILQ_FOREACH(pp, &stylesheets, entry) {
		if (strcmp(pp->str, file) == 0)
			return (0);
	}

	pp = malloc(sizeof *pp);
	pp->str = file;
	TAILQ_INSERT_TAIL(&stylesheets, pp, entry);
	return (0);
}

int
script_add(char *file)
{
	struct page_part	*pp;

	/* FIXME - sanitize input */
	TAILQ_FOREACH(pp, &scripts, entry) {
		if (strcmp(pp->str, file) == 0)
			return (0);
	}

	pp = malloc(sizeof *pp);
	pp->str = file;
	TAILQ_INSERT_TAIL(&scripts, pp, entry);
	return (0);
}

int
page_insert(char *s)
{
	struct page_part	*pp;

	pp = malloc(sizeof *pp);
	pp->str = s;

	TAILQ_INSERT_TAIL(&page_contents, pp, entry);

	return (0);
}

int
page_parse(char *from, char *to)
{
	char		*ptr, *prev_ptr;
	struct tag	*m;

	if (from == NULL)
		return (0);

	if (to)
		*to = '\0';
	prev_ptr = from;
	for (ptr = from; *ptr && ptr != to; ptr++) {
		m = find_tag(ptr);
		if (m) {

			/* push previous data and start-tag to head*/
			*ptr = '\0';
			if (prev_ptr < ptr)
				page_insert(prev_ptr);
			page_insert(m->start_tag);

			/* parse everything inbetween (if anything) */
			if (m->end_ptr) {
				if (m->parse)
					page_parse(m->start_ptr, m->end_ptr);
				else {
					*m->end_ptr = '\0';
					page_insert(m->start_ptr);
				}
			}

			/* push end-tag */
			if (m->end_tag)
				page_insert(m->end_tag);

			prev_ptr = m->skip_ptr;
			ptr = m->skip_ptr - 1;
		}
	}
	page_insert(prev_ptr);
	return (0);
}

char *
page_get()
{
	struct page_part	*pp;
	char			*buf, *ptr;
	int			len = 0;

	/* Calculate size of buffer */
	TAILQ_FOREACH(pp, &page_contents, entry) {
		len += strlen(pp->str);
	}

	if (len == 0)
		return NULL;

	/* FIXME - handle errors */
	buf = malloc(len+1);

	/* Copy all data to buffer */
	ptr = buf;
	TAILQ_FOREACH(pp, &page_contents, entry) {
		strcpy(ptr, pp->str);
		ptr += strlen(ptr);
	}
	*ptr = '\0';
	return buf;
}

int
page_clear()
{
	struct page_part	*pp;

	TAILQ_FOREACH(pp, &page_contents, entry) {
		TAILQ_REMOVE(&page_contents, pp, entry);
		free(pp);
	}

	return (0);
}

int
page_print()
{
	struct page_part	*pp;
	char			*page_title;

	page_title = "blahblah";

	/* Headers */
	webserver_output(
"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \n"
"\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
"<html>\n"
"  <head>\n"
"    <title>%s</title>\n"
"    <meta content=\"text/html; charset=UTF-8\" "
"http-equiv=\"Content-Type\" />\n", page_title);

	TAILQ_FOREACH(pp, &stylesheets, entry) {
		webserver_output("<link href=\"%s\" media=\"all\" rel=\"stylesheet\" "
		    "type=\"text/css\"/>\n", pp->str);
	}

	TAILQ_FOREACH(pp, &scripts, entry) {
		webserver_output("<script type=\"text/javascript\" src=\"%s\">"
		    "</script>\n", pp->str);
	}

	webserver_output("  </head>\n"
	    "<body onload=\"sh_highlightDocument();\">\n"
	    "<div id=\"body\">\n");

	if (header != NULL)
		webserver_output("<div id=\"header\">%s</div>\n", header);

	webserver_output("  <div id=\"contents\">\n");
	TAILQ_FOREACH(pp, &page_contents, entry) {
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

int
page_edit(char *page_data, char *page_name)
{
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

	webserver_output("<link href=\"static/admin.css\" media=\"all\" "
	    "rel=\"stylesheet\" type=\"text/css\"/>\n");

	webserver_output("<script type=\"text/javascript\" src=\"js/admin.js\">"
		    "</script>\n");

	webserver_output("  </head>\n"
	    "<body>\n"
	    "<div id=\"body\">\n");

	if (header != NULL)
		webserver_output("<div id=\"header\">%s</div>\n", header);

	webserver_output("<form method=\"POST\" action=\"?\">\n"
	    "<input type=\"hidden\" name=\"page\" value=\"%s\" />\n"
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

int
page_serve(char *requested_page, int edit_page)
{
	char		*page;
	int		st;

	if (edit_page == 0) {
		/* check if any parts of the page has been updated or
		 * need generating
		 */
		st = wiki_stat_page(requested_page);
		if (st == STAT_PAGE_NO_UPDATES) {
			page = wiki_load_generated(requested_page);
			webserver_output("%s", page);
			free(page);
			return 0;
		}
	}

	init_tags();

	header = NULL;
	footer = NULL;

	TAILQ_INIT(&page_contents);
	TAILQ_INIT(&stylesheets);
	TAILQ_INIT(&scripts);

	if (st & STAT_PAGE_UPDATED_HEADER) {
		/* Generate header */
		/* FIXME - use some kind of lock on
		 * generated.html
		 */
		page = wiki_load_data("_header");
		if (page) {
			page_parse(page, NULL);
			header = page_get();
			wiki_save_generated("_header", header);
			page_clear();
			free(page);
		}
	} else 
		header = wiki_load_generated("_header");

	if (st & STAT_PAGE_UPDATED_FOOTER) {
		/* Generate footer */
		page = wiki_load_data("_footer");
		if (page) {
			page_parse(page, NULL);
			footer = page_get();
			wiki_save_generated("_footer", footer);
			page_clear();
			free(page);
		}
	} else
		footer = wiki_load_generated("_footer");

	/*strdup_sprintf("%s/css/main.css", static_url);*/
	stylesheet_add("static/css/main.css");

	page = wiki_load_data(requested_page);
	if (edit_page)
		page_edit(page, requested_page);
	else {
		page_parse(page, NULL);
		page_print();
	}
	free(page);

	if (header)
		free(header);
	if (footer)
		free(footer);
	return 0;
}
