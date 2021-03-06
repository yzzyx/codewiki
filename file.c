#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include "codewiki.h"

static FILE *
file_get_fd(const char *filename, int create)
{
	struct stat	st;
	FILE		*fd;
	char		*ptr;
	char		*dir;
	char		*mode;

	if (create) {
		/* Create directory structure if needed */
		dir = strdup(filename);
		ptr = strrchr(dir, '/');
		if (ptr == NULL) {
			errno = ENOENT;
			return (NULL);
		}
		*ptr = '\0';

		if (stat(dir, &st) != 0) {
			if (mkdir(dir, 0777) == -1)
				return (NULL);
		} else {
			if (!S_ISDIR(st.st_mode)) {
				errno = ENOENT;
				return (NULL);
			}
		}
		free(dir);

		mode = "w";
	} else
		mode = "r";
	
	if ((fd = fopen(filename, mode)) == NULL)
		return (NULL);
	return (fd);
}

static int
file_get_contents(const char *filename, char **result)
{
	FILE		*fd;
	char		*contents;
	char		*ptr;
	size_t		nb, tot_nb;

	*result = NULL;
	if ((fd = file_get_fd(filename, 0)) == NULL)
		return (-1);

	if ((contents = malloc(BUF_SIZE)) == NULL) {
		fclose(fd);
		return (-1);
	}

	nb = 0;
	tot_nb = 0;
	ptr = contents;
	while(!feof(fd)) {
		nb = fread(ptr, 1, BUF_SIZE, fd);
		if (ferror(fd)) {
			free(contents);
			fclose(fd);
			return (-1);
		}

		tot_nb += nb;
		if (nb < BUF_SIZE) break;

		contents = realloc(contents, tot_nb + BUF_SIZE);
		if (contents == NULL) {
			fclose(fd);
			return (-1);
		}
		ptr = contents + tot_nb;
	}
	ptr[tot_nb] = '\0';

	fclose(fd);

	*result = contents;
	return tot_nb;
}

static int
file_set_contents(const char *filename, const char *contents, size_t len)
{
	FILE		*fd;
	char		*ptr;
	size_t		nb, tot_nb;

	if ((fd = file_get_fd(filename, 1)) == NULL)
		return (-1);

	printf("writing %d bytes to file %s\n", len, filename);
	tot_nb = 0;
	ptr = (char *)contents;
	while (contents && tot_nb < len) {
		nb = fwrite(ptr, 1, len, fd);
		if (ferror(fd) || nb == 0) {
			fclose(fd);
			return (-1);
		}
		tot_nb += nb;
		ptr += nb;
	}

	fclose(fd);
	return (0);
}

static int
mtime_cmp(const char *page)
{
	struct stat st1, st2;
	char		generated[PATH_MAX];
	char		latest[PATH_MAX];

	snprintf(generated, sizeof generated, "%s/%s/generated.html",
	    config.contents_dir, page);

	snprintf(latest, sizeof latest, "%s/%s/latest",
	    config.contents_dir, page);

	if (stat(generated, &st1) == -1)
		return (1);

	if (stat(latest, &st2) == -1)
		return (-1);

	return (st2.st_mtime - st1.st_mtime);
}

int
wiki_stat_page(const char *page)
{
	int	pstat;

	pstat = 0;
	if (mtime_cmp(page) > 0)
		pstat |= STAT_PAGE_UPDATED_PAGE;

	if (mtime_cmp("_header") > 0)
		pstat |= STAT_PAGE_UPDATED_HEADER;

	if (mtime_cmp("_footer") > 0)
		pstat |= STAT_PAGE_UPDATED_FOOTER;

	return (pstat);
}

int
wiki_save_generated(const char *page, const char *contents)
{
	char		filename[PATH_MAX];

	snprintf(filename, sizeof filename, "%s/%s/generated.html",
	    config.contents_dir, page);
	return file_set_contents(filename, contents, strlen(contents));
}

FILE *
wiki_save_generated_fd(const char *page)
{
	char		filename[PATH_MAX];

	snprintf(filename, sizeof filename, "%s/%s/generated.html",
	    config.contents_dir, page);

	return file_get_fd(filename, 1);
}

char *
wiki_load_generated(const char *page)
{
	char		filename[PATH_MAX];
	char		*result;

	snprintf(filename, sizeof filename, "%s/%s/generated.html",
	    config.contents_dir, page);

	file_get_contents(filename, &result);

	return (result);
}

int
wiki_save_data(const char *page, const char *data, int len)
{
	char		filename[PATH_MAX];
	char		new_file[PATH_MAX];
	struct stat	st;

	if (len == -1)
		len = strlen(data);

	snprintf(filename, sizeof filename, "%s/%s/latest",
	    config.contents_dir, page);

	stat(filename, &st);
	snprintf(new_file, sizeof new_file, "%s/%s/%ld",
	    config.contents_dir, page, st.st_mtime);

	/* FIXME - check if file exists already */
	rename(filename, new_file);

	DPRINTF("saving file %s\n", filename);
	return file_set_contents(filename, data, len);
}

int
wiki_load_data(const char *page, char **result)
{
	char		filename[PATH_MAX];

	snprintf(filename, sizeof filename, "%s/%s/latest",
	    config.contents_dir, page);

	DPRINTF("reading file %s\n", filename);
	return file_get_contents(filename, result);
}

char *
wiki_get_data_filename(const char *page)
{
	char		*filename;

	filename = printf_strdup("%s/%s/latest", config.contents_dir, page);
	return (filename);
}

char *
wiki_get_generated_filename(const char *page)
{
	char		*filename;

	filename = printf_strdup("%s/%s/generated.html", config.contents_dir,
	    page);
	return (filename);
}

int
wiki_list_history(const char *page, struct page_part_list *list)
{
	DIR			*d;
	struct dirent		*entry;
	struct dirent		*ptr;
	char			path[PATH_MAX];
	int			cnt, len;
	struct page_part	*pp;
	int			i;

	snprintf(path, sizeof path, "%s/%s/",
	    config.contents_dir, page);

	cnt = 0;
	TAILQ_INIT(list);

	DPRINTF("Listing history in %s\n", path);
	d = opendir(path);
	if (d == NULL)
		return (0);

	len = offsetof(struct dirent, d_name) +
		     pathconf(path, _PC_NAME_MAX) + 1;
	entry = malloc(len);

	if (entry == NULL) {
		closedir(d);
		return (0);
	}

	for (;;) {
		if (readdir_r(d, entry, &ptr) != 0)
			break;
		if (ptr == NULL)
			break;

		DPRINTF("File: %s\n", entry->d_name);
		/* Skip hidden files and '.' and '..' */
		if (entry->d_name[0] == '.')
			continue;

		/* Only look at files */
		if ((entry->d_type & DT_REG) == 0)
			continue;

		/* Only look for all-number filenames */
		for (i = 0; isdigit(entry->d_name[i]); i++);
		if (entry->d_name[i] != '\0')
			continue;

		DPRINTF("Adding to list\n");
		pp = malloc(sizeof *pp);
		pp->str = (char *)strtol(entry->d_name, NULL, 10);
		TAILQ_INSERT_TAIL(list, pp, entry);

		cnt ++;
	}

	closedir(d);
	return (cnt);
}
