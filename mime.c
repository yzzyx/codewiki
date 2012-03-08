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
#include "codewiki.h"

char *
mime_get_key(const char *buf, const char *key)
{
	char	*ptr, *st_ptr;
	char	*value;
	char	end_ch;
	int	len;

	end_ch = '\0';
	value = NULL;
	ptr = (char *)buf;
	for (;;) {
		ptr = strstr(ptr, key);
		if (ptr == NULL)
			break;

		ptr += strlen(key);
		if (*ptr != '=')
			continue;

		ptr ++;
		if (*ptr == '"') {
			end_ch = '"';
			ptr ++;
		}
		st_ptr = ptr;

		for (; *ptr != end_ch && *ptr != '\0'; ptr ++);

		len = ptr - st_ptr;
		value = malloc(len+1);
		memcpy(value, st_ptr, len);
		value[len] = '\0';
		break;
	}

	return (value);
}

char *
mime_getline()
{
	char		*buf;
	int		bufsz;
	int		ch, nb;

	bufsz = 1024;
	nb = 0;

	buf = malloc(bufsz);
	for (;;) {
		ch = webserver_getc();
		if (webserver_eof()) {
			free(buf);
			return (NULL);
		}

		if (ch == '\n' || ch == '\r')
			break;
		buf[nb++] = ch;

		if (nb == bufsz) {
			bufsz += 1024;
			buf = realloc(buf, bufsz);
		}
	}
	buf[nb] = '\0';

	return (buf);
}

/*
 * mime_getdata(boundary,result,result_size)
 *
 * Get next mime-datablock, save it in 'result',
 * and save the size in 'result_size'.
 *
 * Returns:
 * -1 on error, 1 if this was the last block, and 0 if there are more
 */
int
mime_getdata(const char *boundary, char **result, int *result_sz)
{
	char		*buf;
	int		bufsz;
	char		next_boundary[128];
	char		end_boundary[128];
	int		boundary_len;
	int		nb, ch, retval;

	snprintf(next_boundary, sizeof next_boundary, "\r\n%s\r\n", boundary);
	snprintf(end_boundary, sizeof end_boundary, "\r\n%s--", boundary);

	bufsz = 1024;
	nb = 0;

	boundary_len = strlen(next_boundary);

	buf = malloc(bufsz);
	for (;;) {
		ch = webserver_getc();
		if (webserver_eof()) {
			buf[nb] = '\0';
			printf("buf: %s\n",buf);
			free(buf);
			return (-1);
		}

		buf[nb++] = ch;

		if (nb > boundary_len) {
			if (strncmp(buf + nb - boundary_len,
				next_boundary, boundary_len) == 0) {
				/* next boundary */
				retval = 0;
				break;
			} else if (strncmp(buf + nb - boundary_len,
					end_boundary, boundary_len) == 0) {
				/* end boundary */
				retval = 1;
				break;
			}
		}


		if (nb == bufsz) {
			bufsz += 1024;
			buf = realloc(buf, bufsz);
		}
	}
	buf[nb - boundary_len] = '\0';

	*result = buf;
	*result_sz = nb;

	return (retval);
}

int
mime_parse(struct cgi_var_list *cgi_list, const char *boundary)
{
	char			*part_name,
				*part_filename,
				*part_type;
	int			len;
	struct cgi_var		*cv;

	char			*headers;
	char			*ptr;
	char			*data;
	int			ret;

	char			*actual_boundary;

	actual_boundary = alloca(strlen(boundary)+3);
	sprintf(actual_boundary, "--%s", boundary);

	for (;;) {
		part_name = NULL;
		part_filename = NULL;
		part_type = NULL;

		ret = mime_getdata("", &headers, &len);
		if (ret == -1) {
			perror("mime_parse()");
			return (-1);
		}

		printf("Headers: %s\n", headers);
		if ((ptr = strstr(headers, "Content-Disposition: ")) != NULL) {
			ptr += strlen("Content-Disposition: ");
			part_name = mime_get_key(ptr, "name");
			part_filename = mime_get_key(ptr, "filename");
			printf("name: %s\n", part_name);
			printf("filename: %s\n", part_filename);
		}

		if ((ptr = strstr(headers, "Content-Type: ")) != NULL) {
			ptr += strlen("Content-Type: ");
			len = strstr(ptr, "\r\n") - ptr;
			if (len < 0)
				len = strlen(ptr);
			part_type = strndup(ptr, len);
		}
		free(headers);
		printf("End of headers\n");

		ret = mime_getdata(actual_boundary, &data, &len);
		if (ret == -1) {
			perror("mime_getdata():");
			return (0);
		}
		printf("data: %s\n", data);
		if (data != NULL) {
			cv = malloc(sizeof *cv);
			cv->name = part_name;
			cv->value = data;
			cv->value_len = len;
			LIST_INSERT_HEAD(cgi_list, cv, entry);
		}

		if (part_filename)
			free(part_filename);
		if (part_type)
			free(part_type);

		/* if mime_getdata() returned 1, this was the last block */
		if (ret == 1)
			break;
	}
	return (0);
}
