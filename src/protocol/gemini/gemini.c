/* gemini protocol implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "cookies/cookies.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "network/connection.h"
#include "network/progress.h"
#include "network/socket.h"
#include "osdep/ascii.h"
#include "osdep/osdep.h"
#include "osdep/sysname.h"
#include "protocol/date.h"
#include "protocol/gemini/gemini.h"
#include "protocol/header.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "terminal/terminal.h"
#include "util/base64.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


static void done_gemini();
static void gemini_got_header(struct socket *socket, struct read_buffer *rb);

struct module gemini_protocol_module = struct_module(
	/* name: */		N_("Gemini"),
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		done_gemini
);

static void
done_gemini(void)
{
}

struct gemini_connection_info {
	int code;
};

static void
gemini_end_request(struct connection *conn, struct connection_state state,
		 int notrunc)
{
	shutdown_connection_stream(conn);
	abort_connection(conn, state);
}

static void gemini_send_header(struct socket *);

void
gemini_protocol_handler(struct connection *conn)
{
	make_connection(conn->socket, conn->uri, gemini_send_header, 0);
}

static void
done_gemini_connection(struct connection *conn)
{
	struct gemini_connection_info *gemini = conn->info;

	mem_free(gemini);
	conn->info = NULL;
	conn->done = NULL;
}

static struct gemini_connection_info *
init_gemini_connection_info(struct connection *conn)
{
	struct gemini_connection_info *gemini;

	gemini = mem_calloc(1, sizeof(*gemini));
	if (!gemini) {
		gemini_end_request(conn, connection_state(S_OUT_OF_MEM), 0);
		return NULL;
	}

	mem_free_set(&conn->info, gemini);
	conn->done = NULL;//done_gemini_connection;

	return gemini;
}

static void
gemini_send_header(struct socket *socket)
{
	struct connection *conn = socket->conn;
	struct gemini_connection_info *gemini;
	struct string header;
	struct uri *uri = conn->uri;
	char *uristr;

	/* Sanity check for a host */
	if (!uri || !uri->host || !*uri->host || !uri->hostlen) {
		gemini_end_request(conn, connection_state(S_BAD_URL), 0);
		return;
	}

	uristr = get_uri_string(uri, URI_BASE);
	if (!uristr) return;

	gemini = init_gemini_connection_info(conn);
	if (!gemini) return;

	if (!init_string(&header)) {
		gemini_end_request(conn, connection_state(S_OUT_OF_MEM), 0);
		return;
	}

//	if (!conn->cached) conn->cached = find_in_cache(uri);

	add_to_string(&header, uristr);
	mem_free(uristr);
	add_crlf_to_string(&header);

	request_from_socket(socket, header.source, header.length,
				    connection_state(S_SENT),
				    SOCKET_END_ONCLOSE, gemini_got_header);
	done_string(&header);
}

static void read_gemini_data(struct socket *socket, struct read_buffer *rb);

static void
read_more_gemini_data(struct connection *conn, struct read_buffer *rb,
                    int already_got_anything)
{
	struct connection_state state = already_got_anything
		? connection_state(S_TRANS) : conn->state;

	read_from_socket(conn->socket, rb, state, read_gemini_data);
}

static void
read_gemini_data_done(struct connection *conn)
{
	struct gemini_connection_info *gemini = conn->info;

	/* There's no content but an error so just print
	 * that instead of nothing. */
//	if (!conn->from) {
//		if (http->code >= 400) {
//			http_error_document(conn, http->code);
//
//		} else {
//			/* This is not an error, thus fine. No need generate any
//			 * document, as this may be empty and it's not a problem.
//			 * In case of 3xx, we're probably just getting kicked to
//			 * another page anyway. And in case of 2xx, the document
//			 * may indeed be empty and thus the user should see it so. */
//		}
//	}

	gemini_end_request(conn, connection_state(S_OK), 0);
}

/* Returns 0 if more data, 1 if done. */
static int
read_normal_gemini_data(struct connection *conn, struct read_buffer *rb)
{
	struct gemini_connection_info *gemini = conn->info;
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
read_gemini_data(struct socket *socket, struct read_buffer *rb)
{
	struct connection *conn = socket->conn;
	struct gemini_connection_info *gemini = conn->info;
	int ret;

	if (socket->state == SOCKET_CLOSED) {
		read_gemini_data_done(conn);
		return;
	}

	ret = read_normal_gemini_data(conn, rb);

	switch (ret) {
	case 0:
		read_more_gemini_data(conn, rb, 0);
		break;
	case 1:
		read_more_gemini_data(conn, rb, 1);
		break;
	case 2:
		read_gemini_data_done(conn);
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
get_gemini_code(struct read_buffer *rb, int *code)
{
	char *head = rb->data;

	*code = 0;
	if (rb->data[0] < '1' || rb->data[0] > '6') return -1;
	if (rb->data[1] < '0' || rb->data[1] > '9') return -1;

	*code = (rb->data[0] - '0') * 10 + (rb->data[1] - '0');

	return 0;
}

static void
gemini_got_header(struct socket *socket, struct read_buffer *rb)
{
	struct connection *conn = socket->conn;
	struct gemini_connection_info *gemini = conn->info;
	struct string head_string;
	struct uri *uri = conn->uri; /* Set to the real uri */
	struct connection_state state = (!is_in_state(conn->state, S_PROC)
					 ? connection_state(S_GETH)
					 : connection_state(S_PROC));
	int a, h = 20;
	int cf;

	if (socket->state == SOCKET_CLOSED) {
		retry_connection(conn, connection_state(S_CANT_READ));
		return;
	}
	socket->state = SOCKET_END_ONCLOSE;
	conn->cached = get_cache_entry(conn->uri);

again:
	a = get_header(rb);
	if (a == -1) {
		abort_connection(conn, connection_state(S_HTTP_ERROR));
		return;
	}
	if (!a) {
		read_from_socket(conn->socket, rb, state, gemini_got_header);
		return;
	}

	if ((a && get_gemini_code(rb, &h))
	    || ((h >= 40) || h < 10)) {
		abort_connection(conn, connection_state(S_HTTP_ERROR));
		return;
	}

	if (h >= 30 && h < 40) {
		char *url = memacpy(rb->data + 3, a - 4);

		if (!url) {
			abort_connection(conn, connection_state(S_OUT_OF_MEM));
			return;
		}
		redirect_cache(conn->cached, url, 0, 0);
		mem_free(url);
		return;
	}

	init_string(&head_string);
	add_to_string(&head_string, "\nContent-Type:");
	add_bytes_to_string(&head_string, rb->data + 2, a);
	gemini->code = h;

	if (!conn->cached) {
		done_string(&head_string);
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}
	mem_free_set(&conn->cached->head, head_string.source);

	kill_buffer_data(rb, a);
	read_gemini_data(socket, rb);
}
