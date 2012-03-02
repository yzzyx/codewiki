#include <sys/stat.h>
#include <stdio.h>
#include <limits.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include "codewiki.h"

static int
file_get_contents(const char *filename, char **result)
{
	FILE		*fd;
	char		*contents;
	char		*ptr;
	size_t		nb, tot_nb;

	*result = NULL;
	if ((fd = fopen(filename, "r")) == NULL)
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
file_set_contents(const char *filename, const char *contents)
{
	struct stat	st;
	FILE		*fd;
	char		*ptr;
	char		*dir;
	size_t		nb, tot_nb;

	/* Create directory structure if needed */
	dir = strdup(filename);
	ptr = strrchr(dir, '/');
	if (ptr == NULL) {
		errno = ENOENT;
		return (-1);
	}
	*ptr = '\0';

	if (stat(dir, &st) != 0) {
		if (mkdir(dir, 0777) == -1)
			return (-1);
	} else {
		if (!S_ISDIR(st.st_mode)) {
			errno = ENOENT;
			return (-1);
		}
	}
	free(dir);
	
	if ((fd = fopen(filename, "w")) == NULL)
		return (-1);

	tot_nb = 0;
	ptr = (char *)contents;
	while (contents && tot_nb < strlen(contents)) {
		nb = fwrite(ptr, 1, strlen(ptr), fd);
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

int
wiki_stat_page(const char *page)
{
	return STAT_PAGE_UPDATED_PAGE | STAT_PAGE_UPDATED_HEADER |
	    STAT_PAGE_UPDATED_FOOTER;
}

int
wiki_save_generated(const char *page, const char *contents)
{
	char		filename[PATH_MAX];

	snprintf(filename, sizeof filename, "%s/%s/generated.html",
	    CONTENTS_DIR, page);
	return file_set_contents(filename, contents);
}

char *
wiki_load_generated(const char *page)
{
	char		filename[PATH_MAX];
	char		*result;

	snprintf(filename, sizeof filename, "%s/%s/generated.html",
	    CONTENTS_DIR, page);

	file_get_contents(filename, &result);

	return (result);
}

int
wiki_save_data(const char *page, const char *data)
{
	char		filename[PATH_MAX];
	char		new_file[PATH_MAX];
	struct stat	st;

	snprintf(filename, sizeof filename, "%s/%s/latest",
	    CONTENTS_DIR, page);

	stat(filename, &st);
	snprintf(new_file, sizeof new_file, "%s/%s/%ld",
	    CONTENTS_DIR, page, st.st_mtime);

	/* FIXME - check if file exists already */
	rename(filename, new_file);

	DPRINTF("saving file %s\n", filename);
	return file_set_contents(filename, data);
}

int
wiki_load_data(const char *page, char **result)
{
	char		filename[PATH_MAX];

	snprintf(filename, sizeof filename, "%s/%s/latest",
	    CONTENTS_DIR, page);

	DPRINTF("reading file %s\n", filename);
	return file_get_contents(filename, result);
}
