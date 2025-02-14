/* spartan protocol implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "cookies/cookies.h"
#include "intl/charsets.h"
#include "intl/libintl.h"
#include "main/module.h"
#include "network/connection.h"
#include "network/progress.h"
#include "network/socket.h"
#include "osdep/ascii.h"
#include "osdep/osdep.h"
#include "osdep/sysname.h"
#include "protocol/date.h"
#include "protocol/header.h"
#include "protocol/spartan/codes.h"
#include "protocol/spartan/spartan.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "terminal/terminal.h"
#include "util/base64.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


static void done_spartan(struct module *);
static void spartan_got_header(struct socket *socket, struct read_buffer *rb);

struct module spartan_protocol_module = struct_module(
	/* name: */		N_("spartan"),
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		done_spartan,
	/* getname: */	NULL
);

static void
done_spartan(struct module *mod)
{
}

static void
spartan_end_request(struct connection *conn, struct connection_state state,
		 int notrunc)
{
	shutdown_connection_stream(conn);
	abort_connection(conn, state);
}

static void spartan_send_header(struct socket *);

void
spartan_protocol_handler(struct connection *conn)
{
	make_connection(conn->socket, conn->uri, spartan_send_header, 0);
}

static void
done_spartan_connection(struct connection *conn)
{
	struct spartan_connection_info *spartan = (struct spartan_connection_info *)conn->info;

	mem_free_if(spartan->msg);
	mem_free(spartan);
	conn->info = NULL;
	conn->done = NULL;
}

static struct spartan_connection_info *
init_spartan_connection_info(struct connection *conn)
{
	struct spartan_connection_info *spartan;

	spartan = (struct spartan_connection_info *)mem_calloc(1, sizeof(*spartan));
	if (!spartan) {
		spartan_end_request(conn, connection_state(S_OUT_OF_MEM), 0);
		return NULL;
	}

	mem_free_set(&conn->info, spartan);
	conn->done = done_spartan_connection;

	return spartan;
}

static void
spartan_send_header(struct socket *socket)
{
	struct connection *conn = (struct connection *)socket->conn;
	struct spartan_connection_info *spartan;
	struct string header;
	struct uri *uri = conn->uri;
	char *uripath, *urihost;

	/* Sanity check for a host */
	if (!uri || !uri->host || !*uri->host || !uri->hostlen) {
		spartan_end_request(conn, connection_state(S_BAD_URL), 0);
		return;
	}
	urihost = get_uri_string(uri, URI_HOST);
	if (!urihost) {
		return;
	}
	uripath = get_uri_string(uri, URI_PATH);
	if (!uripath) {
		mem_free(urihost);
		return;
	}

	spartan = init_spartan_connection_info(conn);
	if (!spartan) return;

	if (!init_string(&header)) {
		spartan_end_request(conn, connection_state(S_OUT_OF_MEM), 0);
		return;
	}
	add_to_string(&header, urihost);
	add_char_to_string(&header, ' ');
	add_to_string(&header, uripath);
	add_to_string(&header, " 0");
	add_crlf_to_string(&header);
	mem_free(uripath);
	mem_free(urihost);

	request_from_socket(socket, header.source, header.length,
				    connection_state(S_SENT),
				    SOCKET_END_ONCLOSE, spartan_got_header);
	done_string(&header);
}

static void read_spartan_data(struct socket *socket, struct read_buffer *rb);

static void
read_more_spartan_data(struct connection *conn, struct read_buffer *rb,
                    int already_got_anything)
{
	struct connection_state state = already_got_anything
		? connection_state(S_TRANS) : conn->state;

	read_from_socket(conn->socket, rb, state, read_spartan_data);
}

static void
read_spartan_data_done(struct connection *conn)
{
	struct spartan_connection_info *spartan = (struct spartan_connection_info *)conn->info;

	/* There's no content but an error so just print
	 * that instead of nothing. */
	if (!conn->from) {
		if (spartan->code >= 4) {
			spartan_error_document(conn, spartan->msg, spartan->code);
		} else {
			/* This is not an error, thus fine. No need generate any
			 * document, as this may be empty and it's not a problem.
			 * In case of 3xx, we're probably just getting kicked to
			 * another page anyway. And in case of 2xx, the document
			 * may indeed be empty and thus the user should see it so. */
		}
	}

	spartan_end_request(conn, connection_state(S_OK), 0);
}

/* Returns 0 if more data, 1 if done. */
static int
read_normal_spartan_data(struct connection *conn, struct read_buffer *rb)
{
	int data_len;
	int len = rb->length;

	conn->received += len;

	data_len = len;
	add_fragment(conn->cached, conn->from, rb->data, data_len);
	conn->from += data_len;

	kill_buffer_data(rb, len);

	if (conn->socket->state == SOCKET_CLOSED) {
		return 2;
	}

	return !!data_len;
}

static void
read_spartan_data(struct socket *socket, struct read_buffer *rb)
{
	struct connection *conn = (struct connection *)socket->conn;
	int ret;

	if (socket->state == SOCKET_CLOSED) {
		read_spartan_data_done(conn);
		return;
	}

	ret = read_normal_spartan_data(conn, rb);

	switch (ret) {
	case 0:
		read_more_spartan_data(conn, rb, 0);
		break;
	case 1:
		read_more_spartan_data(conn, rb, 1);
		break;
	case 2:
		read_spartan_data_done(conn);
		break;
	default:
		assertm(ret == -1, "Unexpected return value: %d", ret);
		abort_connection(conn, connection_state(S_HTTP_ERROR));
	}
}

/* Returns LF offset */
static int
get_header(struct read_buffer *rb)
{
	int i;

	for (i = 0; i < rb->length; i++) {
		unsigned char a0 = rb->data[i];
		unsigned char a1 = rb->data[i + 1];

		if (a0 == ASCII_CR && i < rb->length - 1) {
			if (a1 != ASCII_LF) return -1;

			return i + 1;
		}
	}

	return 0;
}

static int
get_spartan_code(struct read_buffer *rb)
{
	if (rb->data[0] < '2' || rb->data[0] > '5') return -1;

	return (rb->data[0] - '0');
}

static void
spartan_got_header(struct socket *socket, struct read_buffer *rb)
{
	struct connection *conn = (struct connection *)socket->conn;
	struct spartan_connection_info *spartan = (struct spartan_connection_info *)conn->info;
	struct connection_state state = (!is_in_state(conn->state, S_PROC)
					 ? connection_state(S_GETH)
					 : connection_state(S_PROC));
	int a, h;

	if (socket->state == SOCKET_CLOSED) {
		retry_connection(conn, connection_state(S_CANT_READ));
		return;
	}
	socket->state = SOCKET_END_ONCLOSE;
	conn->cached = get_cache_entry(conn->uri);

	a = get_header(rb);
	if (a == -1) {
		abort_connection(conn, connection_state(S_HTTP_ERROR));
		return;
	}
	if (!a) {
		read_from_socket(conn->socket, rb, state, spartan_got_header);
		return;
	}
	h = get_spartan_code(rb);

	if ((h >= 4) || (h < 2)) {
		spartan->code = h;
		spartan->msg = (h == 4 || h == 5) ? memacpy(rb->data + 2, a - 2) : NULL;
		mem_free_set(&conn->cached->head, stracpy("\nContent-Type: text/html\r\n"));
		read_spartan_data_done(conn);
		return;
	}
	spartan->code = h;

	if (h == 3) {
		char *url = memacpy(rb->data + 2, a - 3);

		if (!url) {
			abort_connection(conn, connection_state(S_OUT_OF_MEM));
			return;
		}
		redirect_cache(conn->cached, url, 0, 0);
		abort_connection(conn, connection_state(S_OK));
		mem_free(url);
		return;
	} else {
		struct string head_string;

		if (!init_string(&head_string)) {
			abort_connection(conn, connection_state(S_OUT_OF_MEM));
			return;
		}
		add_to_string(&head_string, "\nContent-Type: ");
		add_bytes_to_string(&head_string, rb->data + 2, a - 2);

		if (!conn->cached) {
			done_string(&head_string);
			abort_connection(conn, connection_state(S_OUT_OF_MEM));
			return;
		}
		mem_free_set(&conn->cached->head, head_string.source);
	}

	kill_buffer_data(rb, a + 1);
	read_spartan_data(socket, rb);
}
