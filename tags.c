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
#include <limits.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include "codewiki.h"

int generate_tag_br(struct wiki_request *r, struct tag *, char *);
int generate_tag_bold(struct wiki_request *r, struct tag *, char *);
int generate_tag_italic(struct wiki_request *r, struct tag *, char *);
int generate_tag_underline(struct wiki_request *r, struct tag *, char *);
int generate_tag_link(struct wiki_request *r, struct tag *, char *);
int generate_tag_header(struct wiki_request *r, struct tag *, char *);
int generate_tag_image(struct wiki_request *r, struct tag *, char *);
int generate_tag_inline_code(struct wiki_request *r, struct tag *, char *);
int generate_tag_code(struct wiki_request *r, struct tag *, char *);

struct tag_list tags;

struct tag static_tags[] =
{
	{"\n", generate_tag_br },
	{"\r\n", generate_tag_br },
	{"**", generate_tag_bold },
	{"//", generate_tag_italic },
	{"__", generate_tag_underline },
	{"[[", generate_tag_link },
	{"==", generate_tag_header },
	{"{{", generate_tag_image },
	{"<code", generate_tag_code },
	/*
	{"<<", generate_tag_inline_code },
	*/
	{NULL, NULL}
};

int
tags_rb_cmp(struct tag *a, struct tag *b)
{
	char *pa, *pb;

	pa = a->name;
	pb = b->name;

	for (; *pa && *pb && *pa == *pb; pa++, pb++);

	if (*pa == 0 || *pb == 0) return 0;
	if (*pa > *pb) return 1;
	return -1;
}

RB_GENERATE(tag_list, tag, entry, tags_rb_cmp);

int
generate_tag_br(struct wiki_request *r, struct tag *t, char *ptr)
{
	int new_lines;

	new_lines = 0;

	/* only generate tag if we're starting a new paragraph */
	t->skip_ptr = ptr + 1;
	for (; *t->skip_ptr == '\r' ||
	       *t->skip_ptr == '\n' ||
	       *t->skip_ptr == ' '; t->skip_ptr ++)
		if (*t->skip_ptr == '\n') new_lines ++;

	if (new_lines <= 1)
		return (0);

	t->start_tag = "<br />\n<br />\n";
	t->end_tag = NULL;
	t->start_ptr = NULL;
	t->end_ptr = NULL;
	return (1);
}

int
generate_tag_bold(struct wiki_request *r, struct tag *t, char *ptr)
{
	t->start_tag = "<b>";
	t->end_tag = "</b>";

	/* Anything within our tag * should be parsed */
	t->start_ptr = ptr + 2; /* strlen("**"); */
	t->end_ptr = ptr + 2;
	for (; *t->end_ptr && strncmp(t->end_ptr, "**", 2) != 0;
			t->end_ptr ++);

	/* Make sure that we don't print or parse the end-marker */
	t->skip_ptr = t->end_ptr;
	if (*t->end_ptr)
		t->skip_ptr += 2;
	t->parse = 1; /* parse contents */

	return (1);
}

int
generate_tag_italic(struct wiki_request *r, struct tag *t, char *ptr)
{
	t->start_tag = "<i>";
	t->end_tag = "</i>";

	/* Anything within our tag * should be parsed */
	t->start_ptr = ptr + 2; /* strlen("//"); */
	t->end_ptr = ptr + 2;
	for (; *t->end_ptr && strncmp(t->end_ptr, "//", 2) != 0;
			t->end_ptr ++);

	/* Make sure that we don't print or parse the end-marker */
	t->skip_ptr = t->end_ptr;
	if (*t->end_ptr)
		t->skip_ptr += 2;
	t->parse = 1; /* parse contents */

	return (1);
}

int
generate_tag_underline(struct wiki_request *r, struct tag *t, char *ptr)
{
	t->start_tag = "<u>";
	t->end_tag = "</u>";

	/* Anything within our tag * should be parsed */
	t->start_ptr = ptr + 2; /* strlen("__"); */
	t->end_ptr = ptr + 2;
	for (; *t->end_ptr && strncmp(t->end_ptr, "__", 2) != 0;
			t->end_ptr ++);

	/* Make sure that we don't print or parse the end-marker */
	t->skip_ptr = t->end_ptr;
	if (*t->end_ptr)
		t->skip_ptr += 2;
	t->parse = 1; /* parse contents */

	return (1);
}

int
generate_tag_header(struct wiki_request *r, struct tag *t, char *ptr)
{
	t->start_tag = "<h2>";
	t->end_tag = "</h2>";

	/* Anything within our tag * should be parsed */
	t->start_ptr = ptr + 2; /* strlen("=="); */
	t->end_ptr = ptr + 2;
	for (; *t->end_ptr && strncmp(t->end_ptr, "==", 2) != 0;
			t->end_ptr ++);

	/* Make sure that we don't print or parse the end-marker */
	t->skip_ptr = t->end_ptr;
	if (*t->end_ptr)
		t->skip_ptr += 2;
	t->parse = 1; /* parse contents */

	return (1);
}

int
generate_tag_link(struct wiki_request *r, struct tag *t, char *ptr)
{
	char		*tag;
	char		*link, *label;
	char		*format = "<a href=\"%s\">%s</a>";

	/* Find end-tag */
	ptr += 2;
	t->skip_ptr = ptr;
	for (; *t->skip_ptr && strncmp(t->skip_ptr, "]]", 2) != 0;
			t->skip_ptr ++);
	if (*t->skip_ptr == '\0')
		return (0);

	*t->skip_ptr = '\0';
	t->skip_ptr += 2;

	/* Check linktype - it can be one of:
	 * 1) regular [[somepage]]
	 * 2) external [[http://www.kudzu.se]]
	 * 3) interwiki [[wp>WikipediaSomething]]
	 *              [[github>yzzyx/codewiki]]
	 *     FIXME - does not work yet
	 * 4) Any of the above, with label
	 *       [[somepage|click here]]
	 */
	link = ptr;
	label = strchr(ptr, '|');
	if (label == NULL)
		label = ptr;
	else
		*label++ = '\0';

	/* FIXME -
	 * keep track of allocated data - we want to free it
	 * when we're done with it
	 */
	tag = malloc(strlen(link) + strlen(label) + strlen(format));
	sprintf(tag, format, link, label);
	t->start_tag = tag;
	t->end_tag = NULL;
	t->start_ptr = NULL;
	t->end_ptr = NULL;
	t->parse = 0;

	return (1);
}

int
generate_tag_image(struct wiki_request *r, struct tag *t, char *ptr)
{
	char		*tag;
	char		*src, *label;
	char		*format;

	/* Find end-tag */
	ptr += 2;
	t->skip_ptr = ptr;
	for (; *t->skip_ptr && strncmp(t->skip_ptr, "}}", 2) != 0;
			t->skip_ptr ++);
	if (*t->skip_ptr == '\0')
		return (0);

	*t->skip_ptr = '\0';
	t->skip_ptr += 2;


	src = ptr;
	label = strchr(ptr, '|');
	if (label == NULL)
		label = ptr;
	else
		*label++ = '\0';

	/*
	 * External images are handled differently than
	 * internal images
	 */
	if (strncmp(src, "http://", 7) == 0 ||
	    strncmp(src, "https://", 8) == 0)
		format = "<img src=\"%s\" alt=\"%s\" />";
	else
		format = "<img src=\"images/%s\" alt=\"%s\" />";

	/* FIXME -
	 * keep track of allocated data - we want to free it
	 * when we're done with it
	 */
	tag = malloc(strlen(src) + strlen(label) + strlen(format));
	sprintf(tag, format, src, label);
	t->start_tag = tag;
	t->end_tag = NULL;
	t->start_ptr = NULL;
	t->end_ptr = NULL;
	t->parse = 0;

	return (1);
}

int
generate_tag_code(struct wiki_request *r, struct tag *t, char *ptr)
{
	char		*title, *language;
	char		*title_format, *language_format;
	char		*tag, *end_of_tag, *p;
	char		boundary;
	int		len;

	stylesheet_add(r, "/static/css/sh.css");
	script_add(r, "/static/js/sh_main.min.js");

	/* The code tag has the following syntax:
	 *
	 * <code [language] [title="text"]>
	 * some preformatted code
	 * </code>
	 *
	 * if [langugage] is left out, no highlighting will be done
	 */

	ptr += strlen("<code");
	for (; isspace(*ptr); ptr++);
	end_of_tag = ptr;

	/* FIXME - this should be turned into
	 * something that works for all kinds of tags
	 * (and without so much pointer magic)
	 */
	language = NULL;
	title = NULL;
	if (*ptr != '>') {
		for (p = ptr; *p && *p != '>'; p++);
		*p = '\0';
		end_of_tag = p+1;
		
		/* Read title */
		if ((p = strstr(ptr, "title=")) != NULL) {
			p += strlen("title=");

			boundary = ' ';
			if (*p == '"' || *p == '\'')
				boundary = *p++;
			title = p;

			for (; *p && *p != boundary; p++);
			*p = '\0';
		}

		/* Read language */
		for (p = ptr; p != end_of_tag; p++) {
			if (isspace(*p)) {
				for (; isspace(*p); p++);
				if (*p != '>' && strncmp(p, "title=", 6) != 0) {
					language = p;
					for (; !isspace(*p) && *p != '>' &&
						p < end_of_tag; p++);
					*p = '\0';
					break;
				}
			}
		}
	}

	/* FIXME! */
	language = "c";

	/* Calculate how much memory we need */
	title_format = "<span class=\"shtitle\">%s</span>\n";
	language_format = "<pre class=\"sh_%s\">\n";
	len = (title != NULL ? strlen(title_format) + strlen(title) : 0);
	len += (language != NULL ? strlen(language_format) + strlen(language) :
	    strlen("<pre>\n"));

	/* FIXME - make sure this gets a free() somewhere */
	tag = malloc(len);
	tag[0] = '\0';

	if (title != NULL)
		sprintf(tag, title_format, title);

	if (language != NULL) {
		char js_file[PATH_MAX];
		snprintf(js_file, sizeof js_file,
		    "/static/js/sh_lang/sh_%s.min.js", language);
		script_add(r, js_file);

		sprintf(tag + strlen(tag), language_format, language);
	} else
		strcat(tag, "<pre>\n");
	t->start_tag = tag;
	t->end_tag = "</pre>\n";
	t->start_ptr = end_of_tag+1;
	t->end_ptr = strstr(end_of_tag, "</code>");
	if (t->end_ptr == NULL)
		t->skip_ptr = end_of_tag+1;
	else
		t->skip_ptr = t->end_ptr + strlen("</code>");
	t->parse = 0;
	return (1);
}

struct tag *find_tag(struct wiki_request *r, char *p)
{
	struct tag	search;
	struct tag	*m;

	search.name = p;
	m = RB_FIND(tag_list, &tags, &search);

	if (!m)
		return NULL;

	if (m->generate_tag(r, m, p))
		return m;
	return NULL;
}

void
init_tags()
{
	struct tag	*t;
	int		i;

	RB_INIT(&tags);

	for (i=0; static_tags[i].name != NULL; i++) {
		t=&static_tags[i];
		RB_INSERT(tag_list, &tags, t);
	}
}
