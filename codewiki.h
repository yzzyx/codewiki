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
#ifndef __CODEWIKI_H
#define __CODEWIKI_H
#include <sys/types.h>
#include <sys/queue.h>
#include "tree.h"

/* FIXME - configurable settings */
#define CONTENTS_DIR	".contents"
#define BUF_SIZE	8000
#define PTRS_SIZE	100

/* Page */
struct page_part {
	TAILQ_ENTRY(page_part)	entry;
	char			*str;
};
TAILQ_HEAD(page_part_list, page_part);

/* Tags */
struct tag {
	char		*name;

	/* Used when generating data */
	int		(*generate_tag)(struct tag *, char *);
	char		*start_tag;
	char		*end_tag;

	char		*start_ptr;
	char		*end_ptr;
	char		*skip_ptr;
	int		parse;
	RB_ENTRY(tag)	entry;
};
RB_HEAD(tag_list, tag);
RB_PROTOTYPE(tag_list, tag, entry, tags_rb_cmp);

struct tag *find_tag(char *);
void init_tags();

/* Backend */
#define		STAT_PAGE_NO_UPDATES		(0)
#define		STAT_PAGE_UPDATED_PAGE		(1)
#define		STAT_PAGE_UPDATED_HEADER	(2)
#define		STAT_PAGE_UPDATED_FOOTER	(3)
int wiki_stat_page(const char *);
char *wiki_load_generated(const char *);
char *wiki_load_data(const char *);
int wiki_save_generated(const char *, const char *);

/* functions not yet implemented:
int wiki_save_data(const char *, const char *);

int wiki_list_revisions(const char *, struct revision *);
char *wiki_load_revision(const char *, struct revision *)
*/


/* Common */
int stylesheet_add(char *);
int script_add(char *);
int page_serve(char *requested_page, int edit_page);

/* Differs between CGI-implementations */
int webserver_output(char *, ...);
#endif
