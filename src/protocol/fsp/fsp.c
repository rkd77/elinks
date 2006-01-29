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


static void
fsp_error(unsigned char *error)
{
	fprintf(stderr, "text/plain");
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
display_entry(FSP_RDENTRY *fentry, unsigned char dircolor[])
{
	printf("%10d\t<a href=\"%s", fentry->size, fentry->name);
	if (fentry->type == FSP_RDTYPE_DIR) {
		printf("/\">");
		if (*dircolor)
			printf("<font color=\"%s\"><b>", dircolor);
		printf("%s", fentry->name);
		if (*dircolor)
			printf("</b></font>");
	} else {
		printf("\">%s", fentry->name);
	}
	puts("</a>");
}

static void
sort_and_display_entries(FSP_DIR *dir, unsigned char dircolor[])
{
	FSP_RDENTRY fentry, *fresult, *table = NULL;
	int size = 0;
	int i;

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
	qsort(table, size, sizeof(*table),
	 (int (*)(const void *, const void *)) compare);

	for (i = 0; i < size; i++) {
		display_entry(&table[i], dircolor);
	}
}

static void
fsp_directory(FSP_SESSION *ses, struct uri *uri)
{
	struct string buf;
	FSP_DIR *dir;
	unsigned char *uristring = get_uri_string(uri, URI_PUBLIC);
	unsigned char *data = get_uri_string(uri, URI_DATA);
	unsigned char dircolor[8] = "";

	if (!uristring || !data || !init_string(&buf))
		fsp_error("Out of memory");

	fprintf(stderr, "text/html");
	fclose(stderr);
	add_html_to_string(&buf, uristring, strlen(uristring));

	printf("<html><head><title>%s</title><base href=\"%s", buf.source,
		uristring);
	if (buf.source[buf.length - 1] != '/') printf("/");
	printf("\"></head><body><h2>FSP directory %s</h2><pre>", buf.source);

	dir = fsp_opendir(ses, data);
	if (!dir) goto end;

	if (get_opt_bool("document.browse.links.color_dirs")) {
		color_to_string(get_opt_color("document.colors.dirs"),
				(unsigned char *) &dircolor);
	}

	if (get_opt_bool("protocol.fsp.sort")) {
		sort_and_display_entries(dir, dircolor);
	} else {
		FSP_RDENTRY fentry, *fresult;

		while (!fsp_readdir_native(dir, &fentry, &fresult)) {
			if (!fresult) break;
			display_entry(&fentry, dircolor);
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

		/* Use the default way to find the MIME type, so write an
		 * 'empty' name, since something needs to be written in order
		 * to avoid socket errors. */
		fprintf(stderr, "%c", '\0');
		fclose(stderr);

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

	if (!len) {
		if (conn->from)
			normalize_cache_entry(conn->cached, conn->from);
		abort_connection(conn, S_OK);
		return;
	}

	socket->state = SOCKET_END_ONCLOSE;
	conn->received += len;
	if (add_fragment(conn->cached, conn->from, rb->data, len) == 1)
		conn->tries = 0;
	conn->from += len;
	kill_buffer_data(rb, len);

	read_from_socket(socket, rb, S_TRANS, fsp_got_data);
}

static void
fsp_got_header(struct socket *socket, struct read_buffer *rb)
{
	struct connection *conn = socket->conn;
	struct read_buffer *buf;

	conn->cached = get_cache_entry(conn->uri);
	if (!conn->cached) {
		close(socket->fd);
		close(conn->data_socket->fd);
		abort_connection(conn, S_OUT_OF_MEM);
		return;
	}
	socket->state = SOCKET_END_ONCLOSE;

	if (rb->length > 0) {
		unsigned char *ctype = memacpy(rb->data, rb->length);

		if (ctype && *ctype)
			mem_free_set(&conn->cached->content_type, ctype);
		else
			mem_free_if(ctype);
	}

	buf = alloc_read_buffer(conn->data_socket);
	if (!buf) {
		close(socket->fd);
		close(conn->data_socket->fd);
		abort_connection(conn, S_OUT_OF_MEM);
		return;
	}
	read_from_socket(conn->data_socket, buf, S_CONN, fsp_got_data);
}

#undef READ_SIZE


void
fsp_protocol_handler(struct connection *conn)
{
	int fsp_pipe[2] = { -1, -1 };
	int header_pipe[2] = { -1, -1 };
	pid_t cpid;

	if (c_pipe(fsp_pipe) || c_pipe(header_pipe)) {
		int s_errno = errno;

		if (fsp_pipe[0] >= 0) close(fsp_pipe[0]);
		if (fsp_pipe[1] >= 0) close(fsp_pipe[1]);
		if (header_pipe[0] >= 0) close(header_pipe[0]);
		if (header_pipe[1] >= 0) close(header_pipe[1]);
		abort_connection(conn, -s_errno);
		return;
	}
	conn->from = 0;
	conn->unrestartable = 1;

	cpid = fork();
	if (cpid == -1) {
		int s_errno = errno;

		close(fsp_pipe[0]);
		close(fsp_pipe[1]);
		close(header_pipe[0]);
		close(header_pipe[1]);
		retry_connection(conn, -s_errno);
		return;
	}

	if (!cpid) {
		close(1);
		dup2(fsp_pipe[1], 1);
		close(0);
		dup2(open("/dev/null", O_RDONLY), 0);
		close(2);
		dup2(header_pipe[1], 2);
		close(fsp_pipe[0]);
		close(header_pipe[0]);

		close_all_non_term_fd();
		do_fsp(conn);

	} else {
		struct read_buffer *buf2;

		conn->data_socket->fd = fsp_pipe[0];
		conn->socket->fd = header_pipe[0];
		close(fsp_pipe[1]);
		close(header_pipe[1]);
		buf2 = alloc_read_buffer(conn->socket);
		if (!buf2) {
			close(fsp_pipe[0]);
			close(header_pipe[0]);
			abort_connection(conn, S_OUT_OF_MEM);
			return;
		}
		read_from_socket(conn->socket, buf2, S_CONN, fsp_got_header);
	}
}
