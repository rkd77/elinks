/* Internal "finger" protocol implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "network/connection.h"
#include "network/socket.h"
#include "protocol/finger/finger.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "util/memory.h"
#include "util/string.h"

struct module finger_protocol_module = struct_module(
	/* name: */		N_("Finger"),
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);

static void
finger_end_request(struct connection *conn, enum connection_state state)
{
	if (state == S_OK && conn->cached)
		normalize_cache_entry(conn->cached, conn->from);

	abort_connection(conn, state);
}

static void
finger_get_response(struct socket *socket, struct read_buffer *rb)
{
	struct connection *conn = socket->conn;
	struct cache_entry *cached = get_cache_entry(conn->uri);
	int l;

	if (!cached) {
		abort_connection(conn, S_OUT_OF_MEM);
		return;
	}
	conn->cached = cached;

	if (socket->state == SOCKET_CLOSED) {
		finger_end_request(conn, S_OK);
		return;
	}

	l = rb->length;
	conn->received += l;

	if (add_fragment(conn->cached, conn->from, rb->data, l) == 1)
		conn->tries = 0;

	conn->from += l;
	kill_buffer_data(rb, l);
	read_from_socket(conn->socket, rb, S_TRANS, finger_get_response);
}

static void
finger_send_request(struct socket *socket)
{
	struct connection *conn = socket->conn;
	struct string req;

	if (!init_string(&req)) return;
	/* add_to_string(&req, &rl, "/W"); */

	if (conn->uri->user) {
		add_char_to_string(&req, ' ');
		add_bytes_to_string(&req, conn->uri->user, conn->uri->userlen);
	}
	add_crlf_to_string(&req);
	request_from_socket(socket, req.source, req.length, S_SENT,
			    SOCKET_END_ONCLOSE, finger_get_response);
	done_string(&req);
}

void
finger_protocol_handler(struct connection *conn)
{
	conn->from = 0;
	make_connection(conn->socket, conn->uri, finger_send_request,
			conn->cache_mode >= CACHE_MODE_FORCE_RELOAD);
}
