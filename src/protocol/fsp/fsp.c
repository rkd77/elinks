/* Internal FSP protocol implementation */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* Needed for asprintf() */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <fsplib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "main/select.h"
#include "network/connection.h"
#include "network/socket.h"
#include "osdep/osdep.h"
#include "protocol/common.h"
#include "protocol/protocol.h"
#include "protocol/fsp/fsp.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/snprintf.h"
#include "util/string.h"


struct option_info fsp_options[] = {
	INIT_OPT_TREE("protocol", N_("FSP"),
		"fsp", 0,
		N_("FSP specific options.")),

	INIT_OPT_BOOL("protocol.fsp", N_("Sort entries"),
		"sort", 0, 1,
		N_("Whether to sort entries in directory listings.")),

	NULL_OPTION_INFO,
};

struct module fsp_protocol_module = struct_module(
	/* name: */		N_("FSP"),
	/* options: */		fsp_options,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);


struct fsp_info {
	int init;
};

static void
fsp_error(unsigned char *error)
{
	puts(error);
	exit(1);
}

static int
compare(FSP_RDENTRY *a, FSP_RDENTRY *b)
{
	int res = ((b->type == FSP_RDTYPE_DIR) - (a->type == FSP_RDTYPE_DIR));

	if (res)
		return res;
	return strcmp(a->name, b->name);
}


static void
sort_and_display_entries(FSP_DIR *dir)
{
	FSP_RDENTRY fentry, *fresult, *table = NULL;
	int size = 0;
	int i;
	unsigned char dircolor[8];

	if (get_opt_bool("document.browse.links.color_dirs")) {
		color_to_string(get_opt_color("document.colors.dirs"),
			(unsigned char *) &dircolor);
	} else {
		dircolor[0] = 0;
	}

	while (!fsp_readdir_native(dir, &fentry, &fresult)) {
		FSP_RDENTRY *new_table;

		if (!fresult) break;
		if (!strcmp(fentry.name, "."))
			continue;
		new_table = mem_realloc(table, (size + 1) * sizeof(*table));
		if (!new_table)
			continue;
		table = new_table;
		memcpy(&table[size], &fentry, sizeof(fentry));
		size++;
	}
	qsort(table, size, sizeof(fentry),
	 (int (*)(const void *, const void *)) compare);

	for (i = 0; i < size; i++) {
		printf("%10d\t<a href=\"%s\">", table[i].size, table[i].name);
		if (fentry.type == FSP_RDTYPE_DIR && *dircolor) {
			printf("<font color=\"%s\"><b>", dircolor);
		}
		printf("%s", table[i].name);
		if (fentry.type == FSP_RDTYPE_DIR && *dircolor) {
			printf("</b></font>");
		}
		puts("</a>");
	}
}

static void
fsp_directory(FSP_SESSION *ses, struct uri *uri)
{
	struct string buf;
	FSP_DIR *dir;
	unsigned char *uristring = get_uri_string(uri, URI_PUBLIC);
	unsigned char *data = get_uri_string(uri, URI_DATA);

	if (!uristring || !data || !init_string(&buf))
		fsp_error("Out of memory");

	add_html_to_string(&buf, uristring, strlen(uristring));

	printf("<html><head><title>%s</title><base href=\"%s\">"
	       "</head><body><h2>FSP directory %s</h2><pre>",
	       buf.source, uristring, buf.source);

	dir = fsp_opendir(ses, data);
	if (!dir) goto end;

	if (get_opt_bool("protocol.fsp.sort")) {
		sort_and_display_entries(dir);
	} else {
		FSP_RDENTRY fentry, *fresult;
		unsigned char dircolor[8];

		if (get_opt_bool("document.browse.links.color_dirs")) {
			color_to_string(get_opt_color("document.colors.dirs"),
				(unsigned char *) &dircolor);
		} else {
			dircolor[0] = 0;
		}

		while (!fsp_readdir_native(dir, &fentry, &fresult)) {
			if (!fresult) break;
			printf("%10d\t<a href=\"%s\">", fentry.size, fentry.name);
			if (fentry.type == FSP_RDTYPE_DIR && *dircolor) {
				printf("<font color=\"%s\"><b>", dircolor);
			}
			printf("%s", fentry.name);
			if (fentry.type == FSP_RDTYPE_DIR && *dircolor) {
				printf("</b></font>");
			}
			puts("</a>");
		}
		fsp_closedir(dir);
	}
end:
	puts("</pre><hr></body></html>");
	fsp_close_session(ses);
	exit(0);
}

#define READ_SIZE	4096

static void
do_fsp(struct connection *conn)
{
	struct stat sb;
	struct uri *uri = conn->uri;
	unsigned char *host = get_uri_string(uri, URI_HOST);
	unsigned char *password = get_uri_string(uri, URI_PASSWORD);
	unsigned char *data = get_uri_string(uri, URI_DATA);
	unsigned short port = (unsigned short)get_uri_port(uri);
	FSP_SESSION *ses = fsp_open_session(host, port, password);

	if (!ses)
		fsp_error("Session initialization failed.");
	if (fsp_stat(ses, data, &sb))
		fsp_error("File not found.");
	if (S_ISDIR(sb.st_mode))
		fsp_directory(ses, uri);
	else { /* regular file */
		char buf[READ_SIZE];
		FSP_FILE *file = fsp_fopen(ses, data, "r");
		int r;

		if (!file)
			fsp_error("fsp_fopen error.");

		while ((r = fsp_fread(buf, 1, READ_SIZE, file)) > 0)
			fwrite(buf, 1, r, stdout);
		fsp_fclose(file);
		fsp_close_session(ses);
		exit(0);
	}
}

static void
fsp_got_data(struct socket *socket, struct read_buffer *rb)
{
	int len = rb->length;
	struct connection *conn = socket->conn;

	if (len < 0) {
		abort_connection(conn, -errno);
		return;
	}

	if (len == 0) {
		if (conn->from)
			normalize_cache_entry(conn->cached, conn->from);
		abort_connection(conn, S_OK);
		return;
	}

	conn->socket->state = SOCKET_END_ONCLOSE;
	conn->received += len;
	if (add_fragment(conn->cached, conn->from, rb->data, len) == 1)
		conn->tries = 0;
	conn->from += len;
	kill_buffer_data(rb, len);

	read_from_socket(socket, rb, S_TRANS, fsp_got_data);
}

#undef READ_SIZE


void
fsp_protocol_handler(struct connection *conn)
{
	int fsp_pipe[2] = { -1, -1 };
	pid_t cpid;
	struct read_buffer *buf;

	if (c_pipe(fsp_pipe)) {
		int s_errno = errno;

		if (fsp_pipe[0] >= 0) close(fsp_pipe[0]);
		if (fsp_pipe[1] >= 0) close(fsp_pipe[1]);
		abort_connection(conn, -s_errno);
		return;
	}

	conn->cached = get_cache_entry(conn->uri);
	if (!conn->cached) {
		abort_connection(conn, S_OUT_OF_MEM);
		return;
	}
	conn->from = 0;
	conn->unrestartable = 1;

	cpid = fork();
	if (cpid == -1) {
		int s_errno = errno;

		close(fsp_pipe[0]);
		close(fsp_pipe[1]);
		retry_connection(conn, -s_errno);
		return;
	}

	if (!cpid) {
		close(1);
		dup2(fsp_pipe[1], 1);
		close(0);
		dup2(open("/dev/null", O_RDONLY), 0);
		close(2);
		dup2(open("/dev/null", O_RDONLY), 2);
		close(fsp_pipe[0]);

		close_all_non_term_fd();
		do_fsp(conn);

	} else {
		conn->socket->fd = fsp_pipe[0];
		close(fsp_pipe[1]);
		buf = alloc_read_buffer(conn->socket);
		if (!buf) {
			close(fsp_pipe[0]);
			abort_connection(conn, S_OUT_OF_MEM);
			return;
		}
		read_from_socket(conn->socket, buf, S_CONN, fsp_got_data);
	}
}
