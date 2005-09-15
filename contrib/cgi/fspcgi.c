/* CGI script for FSP protocol support */
/* $Id: fspcgi.c,v 1.8 2005/06/29 19:53:18 witekfl Exp $ */

#include <ctype.h>
#include <fsplib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

char *pname, *query;

struct fq {
	char *password;
	char *host;
	char *path;
	unsigned short port;
} data;

static void
error(const char *str)
{
	printf("Content-Type: text/plain\r\nConnection: close\r\n\r\n");
	puts(str);
	printf("%s\n", query);
	exit(1);
}

static void
process_directory(FSP_SESSION *ses)
{
	char buf[1024];
	FSP_DIR *dir;
	/* TODO: password */

	snprintf(buf, sizeof(buf), "file://%s?%s:%d%s/", pname, data.host,
		data.port, data.path);
	printf("Content-Type: text/html\r\n\r\n");
	printf("<html><head><title>%s</title></head><body>\n", buf);
	dir = fsp_opendir(ses, data.path);
	if (dir) {
		FSP_RDENTRY fentry, *fresult;

		while (!fsp_readdir_native(dir, &fentry, &fresult)) {
			if (!fresult) break;
			printf("<a href=\"%s%s\">%s</a><br>\n", buf, fentry.name, fentry.name);
		}
		fsp_closedir(dir);
	}	
	puts("</body></html>");
	fsp_close_session(ses);
	exit(0);
}

static void
process_data(void)
{
	FSP_SESSION *ses = fsp_open_session(data.host, data.port, data.password);
	struct stat sb;

	if (!ses) error("Session initialization failed.");
	if (fsp_stat(ses, data.path, &sb)) error("File not found.");
	if (S_ISDIR(sb.st_mode)) process_directory(ses);
	else { /* regular file */
		char buf[4096];
		FSP_FILE *file = fsp_fopen(ses, data.path, "r");
		int r;

		if (!file) error("fsp_fopen error.");
		printf("Content-Type: application/octet-stream\r\nContent-Length: %d\r\n"
			"Connection: close\r\n\r\n", sb.st_size);
		while ((r = fsp_fread(buf, 1, 4096, file)) > 0) fwrite(buf, 1, r, stdout);
		fsp_fclose(file);
		fsp_close_session(ses);
		exit(0);
	}
}

static void
process_query(void)
{
	char *at = strchr(query, '@');
	char *colon;
	char *slash;

	if (at) {
		*at = '\0';
		data.password = strdup(query);
		query = at + 1;
	}
	colon = strchr(query, ':');
	if (colon) {
		*colon = '\0';
		data.host = strdup(query);
		data.port = atoi(colon + 1);
		slash = strchr(colon + 1, '/');
		if (slash) {
			data.path = strdup(slash);
		} else {
			data.path = "/";
		}
	} else {
		data.port = 21;
		slash = strchr(query, '/');
		if (slash) {
			*slash = '\0';
			data.host = strdup(query);
			*slash = '/';
			data.path = strdup(slash);
		} else {
			data.host = strdup(query);
			data.path = "/";
		}
	}
	process_data();
}

static inline int
unhx(register unsigned char a)
{
	if (isdigit(a)) return a - '0';
	if (a >= 'a' && a <= 'f') return a - 'a' + 10;
	if (a >= 'A' && a <= 'F') return a - 'A' + 10;
	return -1;
}

static void
decode_query(char *src)
{
	char *dst = src;
	char c;

	do {
		c = *src++;

		if (c == '%') {
			int x1 = unhx(*src);

			if (x1 >= 0) {
				int x2 = unhx(*(src + 1));

				if (x2 >= 0) {
					x1 = (x1 << 4) + x2;
					if (x1 != 0) { /* don't allow %00 */
						c = (unsigned char) x1;
						src += 2;
					}
				}
			}	

		} else if (c == '+') {
			c = ' ';
		}

		*dst++ = c;
	} while (c != '\0');
}

int
main(int argc, char **argv)
{
	char *q = getenv("QUERY_STRING");

	if (!q) return 1;
	pname = argv[0];
	query = strdup(q);
	if (!query) return 2;
	decode_query(query);
	process_query();
	return 0;
}
