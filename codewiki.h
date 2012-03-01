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
#include "config.h"
#include <sys/types.h>

#if HAVE_QUEUE_H
#include <queue.h>
#elif HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#elif HAVE_BSD_QUEUE_H
#include <bsd/queue.h>
#else
#include "queue.h" /* Use local implementation */
#endif

#if HAVE_TREE_H
#include <sys/tree.h>
#elif HAVE_BSD_TREE_H
#include <bsd/sys/tree.h>
#else
#include "tree.h" /* Use local implementation */
#endif

/* FIXME - configurable settings */
#define CONTENTS_DIR	".contents"
#define BUF_SIZE	8000
#define PTRS_SIZE	100

#ifdef DEBUG
#define DPRINTF(x...)		fprintf(stderr, x)
#else
#define DPRINTF(x...)
#endif

/* Page */
struct page_part {
	TAILQ_ENTRY(page_part)	entry;
	char			*str;
};
TAILQ_HEAD(page_part_list, page_part);

struct wiki_request {
	char			*mime_type;
	char			*data;
	char			*requested_page;
	int			edit;

	struct page_part_list	page_contents;
	struct page_part_list	stylesheets;
	struct page_part_list	scripts;
};


/* Tags */
struct tag {
	char		*name;

	/* Used when generating data */
	int		(*generate_tag)(struct wiki_request *,
					struct tag *, char *);
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

struct tag *find_tag(struct wiki_request *, char *);
void init_tags();

/* Backend */
struct config {
	char		*static_url;
	char		*base_url;
};
extern struct config config;

#define STAT_PAGE_NO_UPDATES		(0)
#define STAT_PAGE_UPDATED_PAGE		(1)
#define STAT_PAGE_UPDATED_HEADER	(2)
#define STAT_PAGE_UPDATED_FOOTER	(3)
int wiki_stat_page(const char *);
char *wiki_load_generated(const char *);
char *wiki_load_data(const char *);
int wiki_save_data(const char *, const char *);
int wiki_save_generated(const char *, const char *);

#define WIKI_LOGIN_OK			(0)
#define WIKI_LOGIN_WRONG_PASSWORD	(1)
#define WIKI_LOGIN_ERROR		(2)
int wiki_login(const char *,const char *);
char *wiki_ticket_get(const char *);

#define WIKI_TICKET_READ		(1)
#define WIKI_TICKET_WRITE		(2)
#define WIKI_TICKET_GRANT		(4)
#define WIKI_TICKET_ADMIN		(8)
int wiki_ticket_access(const char *, const char *);
int wiki_ticket_clear(const char *);

/* functions not yet implemented:
int wiki_save_data(const char *, const char *);
int wiki_list_revisions(const char *, struct revision *);
char *wiki_load_revision(const char *, struct revision *)
*/


/* Common */
int stylesheet_add(struct wiki_request *r, char *file);
int script_add(struct wiki_request *r, char *file);

int wiki_init();
int wiki_load_config();
struct wiki_request *wiki_request_new();
int wiki_request_serve(struct wiki_request *);
int wiki_request_clear(struct wiki_request *);

/* Differs between CGI-implementations */
int webserver_output(const char *, ...);
int webserver_output_buf(const char *, int);
#endif
