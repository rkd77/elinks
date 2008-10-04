/* Connections management */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "document/document.h"
#include "encoding/encoding.h"
#include "intl/gettext/libintl.h"
#include "main/object.h"
#include "main/select.h"
#include "main/timer.h"
#include "network/connection.h"
#include "network/dns.h"
#include "network/progress.h"
#include "network/socket.h"
#include "network/ssl/ssl.h"
#include "protocol/protocol.h"
#include "protocol/proxy.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "util/time.h"


struct keepalive_connection {
	LIST_HEAD(struct keepalive_connection);

	/* XXX: This is just the URI of the connection that registered the
	 * keepalive connection so only rely on the protocol, user, password,
	 * host and port part. */
	struct uri *uri;

	/* Function called when the keepalive has timed out or is deleted */
	void (*done)(struct connection *);

	timeval_T timeout;
	timeval_T creation_time;

	unsigned int protocol_family:1; /* see network/socket.h, EL_PF_INET, EL_PF_INET6 */
	int socket;
};


static unsigned int connection_id = 0;
static int active_connections = 0;
static timer_id_T keepalive_timeout = TIMER_ID_UNDEF;

static INIT_LIST_OF(struct connection, connection_queue);
static INIT_LIST_OF(struct host_connection, host_connections);
static INIT_LIST_OF(struct keepalive_connection, keepalive_connections);

/* Prototypes */
static void notify_connection_callbacks(struct connection *conn);
static void check_keepalive_connections(void);


static /* inline */ enum connection_priority
get_priority(struct connection *conn)
{
	enum connection_priority priority;

	for (priority = 0; priority < PRIORITIES; priority++)
		if (conn->pri[priority])
			break;

	assertm(priority != PRIORITIES, "Connection has no owner");
	/* Recovery path ;-). (XXX?) */

	return priority;
}

int
get_connections_count(void)
{
	return list_size(&connection_queue);
}

int
get_keepalive_connections_count(void)
{
	return list_size(&keepalive_connections);
}

int
get_connections_connecting_count(void)
{
	struct connection *conn;
	int i = 0;

	foreach (conn, connection_queue)
		i += is_in_connecting_state(conn->state);

	return i;
}

int
get_connections_transfering_count(void)
{
	struct connection *conn;
	int i = 0;

	foreach (conn, connection_queue)
		i += is_in_transfering_state(conn->state);

	return i;
}

/** Check whether the pointer @a conn still points to a connection
 * with the given @a id.  If the struct connection has already been
 * freed, this returns 0.  By comparing connection.id, this function
 * can usually detect even the case where a different connection has
 * been created at the same address.  For that to work, the caller
 * must save the connection.id before the connection can be deleted.  */
static inline int
connection_disappeared(struct connection *conn, unsigned int id)
{
	struct connection *c;

	foreach (c, connection_queue)
		if (conn == c && id == c->id)
			return 0;

	return 1;
}

/* Host connection management: */
/* Used to keep track on the number of connections to any given host. When
 * trying to setup a new connection the list is searched to see if the maximum
 * number of connection has been reached. If that is the case we try to suspend
 * an already established connection. */
/* Some connections (like file://) that do not involve hosts are not maintained
 * in the list. */

struct host_connection {
	OBJECT_HEAD(struct host_connection);

	/* XXX: This is just the URI of the connection that registered the
	 * host connection so only rely on the host part. */
	struct uri *uri;
};

static struct host_connection *
get_host_connection(struct connection *conn)
{
	struct host_connection *host_conn;

	if (!conn->uri->host) return NULL;

	foreach (host_conn, host_connections)
		if (compare_uri(host_conn->uri, conn->uri, URI_HOST))
			return host_conn;

	return NULL;
}

/* Returns if the connection was successfully added. */
/* Don't add hostnameless host connections but they're valid. */
static int
add_host_connection(struct connection *conn)
{
	struct host_connection *host_conn = get_host_connection(conn);

	if (!host_conn && conn->uri->host) {
		host_conn = mem_calloc(1, sizeof(*host_conn));
		if (!host_conn) return 0;

		host_conn->uri = get_uri_reference(conn->uri);
		object_nolock(host_conn, "host_connection");
		add_to_list(host_connections, host_conn);
	}
	if (host_conn) object_lock(host_conn);

	return 1;
}

/* Decrements and free()s the host connection if it is the last 'refcount'. */
static void
done_host_connection(struct connection *conn)
{
	struct host_connection *host_conn = get_host_connection(conn);

	if (!host_conn) return;

	object_unlock(host_conn);
	if (is_object_used(host_conn)) return;

	del_from_list(host_conn);
	done_uri(host_conn->uri);
	mem_free(host_conn);
}


static void sort_queue();

#ifdef CONFIG_DEBUG
static void
check_queue_bugs(void)
{
	struct connection *conn;
	enum connection_priority prev_priority = 0;
	int cc = 0;

	foreach (conn, connection_queue) {
		enum connection_priority priority = get_priority(conn);

		cc += conn->running;

		assertm(priority >= prev_priority, "queue is not sorted");
		assertm(is_in_progress_state(conn->state),
			"interrupted connection on queue (conn %s, state %d)",
			struri(conn->uri), conn->state);
		prev_priority = priority;
	}

	assertm(cc == active_connections,
		"bad number of active connections (counted %d, stored %d)",
		cc, active_connections);
}
#else
#define check_queue_bugs()
#endif

static void
set_connection_socket_state(struct socket *socket, struct connection_state state)
{
	assert(socket);
	set_connection_state(socket->conn, state);
}

static void
set_connection_socket_timeout(struct socket *socket, struct connection_state state)
{
	assert(socket);
	set_connection_timeout(socket->conn);
}

static void
retry_connection_socket(struct socket *socket, struct connection_state state)
{
	assert(socket);
	retry_connection(socket->conn, state);
}

static void
done_connection_socket(struct socket *socket, struct connection_state state)
{
	assert(socket);
	abort_connection(socket->conn, state);
}

static struct connection *
init_connection(struct uri *uri, struct uri *proxied_uri, struct uri *referrer,
		off_t start, enum cache_mode cache_mode,
		enum connection_priority priority)
{
	static struct socket_operations connection_socket_operations = {
		set_connection_socket_state,
		set_connection_socket_timeout,
		retry_connection_socket,
		done_connection_socket,
	};
	struct connection *conn = mem_calloc(1, sizeof(*conn));

	if (!conn) return NULL;

	assert(proxied_uri->protocol != PROTOCOL_PROXY);

	conn->socket = init_socket(conn, &connection_socket_operations);
	if (!conn->socket) {
		mem_free(conn);
		return NULL;
	}

	conn->data_socket = init_socket(conn, &connection_socket_operations);
	if (!conn->data_socket) {
		mem_free(conn->socket);
		mem_free(conn);
		return NULL;
	}

	conn->progress = init_progress(start);
	if (!conn->progress) {
		mem_free(conn->data_socket);
		mem_free(conn->socket);
		mem_free(conn);
		return NULL;
	}

	/* load_uri() gets the URI from get_proxy() which grabs a reference for
	 * us. */
	conn->uri = uri;
	conn->proxied_uri = proxied_uri;
	conn->id = connection_id++;
	conn->pri[priority] = 1;
	conn->cache_mode = cache_mode;

	conn->content_encoding = ENCODING_NONE;
	conn->stream_pipes[0] = conn->stream_pipes[1] = -1;
	init_list(conn->downloads);
	conn->est_length = -1;
	conn->timer = TIMER_ID_UNDEF;

	if (referrer) {
		/* Don't set referrer when it is the file protocol and the URI
		 * being loaded is not. This means CGI scripts will have it
		 * available while preventing information about the local
		 * system from being leaked to external servers. */
		if (referrer->protocol != PROTOCOL_FILE
		    || uri->protocol == PROTOCOL_FILE)
			conn->referrer = get_uri_reference(referrer);
	}

	return conn;
}

static void
update_connection_progress(struct connection *conn)
{
	update_progress(conn->progress, conn->received, conn->est_length, conn->from);
}

/* Progress timer callback for @conn->progress.  As explained in
 * @start_update_progress, this function must erase the expired timer
 * ID from @conn->progress->timer.  */
static void
stat_timer(struct connection *conn)
{
	update_connection_progress(conn);
	/* The expired timer ID has now been erased.  */
	notify_connection_callbacks(conn);
}

void
set_connection_state(struct connection *conn, struct connection_state state)
{
	struct download *download;
	struct progress *progress = conn->progress;

	if (is_in_result_state(conn->state) && is_in_progress_state(state))
		conn->prev_error = conn->state;

	conn->state = state;
	if (is_in_state(conn->state, S_TRANS)) {
		if (progress->timer == TIMER_ID_UNDEF) {
			const unsigned int id = conn->id;

			start_update_progress(progress, (void (*)(void *)) stat_timer, conn);
			update_connection_progress(conn);
			if (connection_disappeared(conn, id))
				return;
		}

	} else {
		kill_timer(&progress->timer);
	}

	foreach (download, conn->downloads) {
		download->state = state;
		download->prev_error = conn->prev_error;
	}

	if (is_in_progress_state(state)) notify_connection_callbacks(conn);
}

void
shutdown_connection_stream(struct connection *conn)
{
	if (conn->stream) {
		close_encoded(conn->stream);
		conn->stream = NULL;
	} else if (conn->stream_pipes[0] >= 0) {
		/* close_encoded() usually closes this end of the pipe,
		 * but open_encoded() apparently failed this time.  */
		close(conn->stream_pipes[0]);
	}
	if (conn->stream_pipes[1] >= 0)
		close(conn->stream_pipes[1]);
	conn->stream_pipes[0] = conn->stream_pipes[1] = -1;
}

static void
free_connection_data(struct connection *conn)
{
	assertm(conn->running, "connection already suspended");
	/* XXX: Recovery path? Originally, there was none. I think we'll get
	 * at least active_connections underflows along the way. --pasky */
	conn->running = 0;

	active_connections--;
	assertm(active_connections >= 0, "active connections underflow");
	if_assert_failed active_connections = 0;

#ifdef CONFIG_SSL
	if (conn->socket->ssl && conn->cached)
		mem_free_set(&conn->cached->ssl_info, get_ssl_connection_cipher(conn->socket));
#endif

	if (conn->done)
		conn->done(conn);

	done_socket(conn->socket);
	done_socket(conn->data_socket);

	shutdown_connection_stream(conn);

	mem_free_set(&conn->info, NULL);

	kill_timer(&conn->timer);

	if (!is_in_state(conn->state, S_WAIT))
		done_host_connection(conn);
}

void
notify_connection_callbacks(struct connection *conn)
{
	struct connection_state state = conn->state;
	unsigned int id = conn->id;
	struct download *download, *next;

	foreachsafe (download, next, conn->downloads) {
		download->cached = conn->cached;
		if (download->callback)
			download->callback(download, download->data);
		if (is_in_progress_state(state)
		    && connection_disappeared(conn, id))
			return;
	}
}

static void
done_connection(struct connection *conn)
{
	/* When removing the connection callbacks should always be aware of it
	 * so they can unregister themselves. We do this by enforcing that the
	 * connection is in a result state. If it is not already it is an
	 * internal bug. This should never happen but it does. ;) --jonas */
	if (!is_in_result_state(conn->state))
		set_connection_state(conn, connection_state(S_INTERNAL));

	del_from_list(conn);
	notify_connection_callbacks(conn);
	if (conn->referrer) done_uri(conn->referrer);
	done_uri(conn->uri);
	done_uri(conn->proxied_uri);
	mem_free(conn->socket);
	mem_free(conn->data_socket);
	done_progress(conn->progress);
	mem_free(conn);
	check_queue_bugs();
}

static inline void
add_to_queue(struct connection *conn)
{
	struct connection *c;
	enum connection_priority priority = get_priority(conn);

	foreach (c, connection_queue)
		if (get_priority(c) > priority)
			break;

	add_at_pos(c->prev, conn);
}


/* Returns zero if no callback was done and the keepalive connection should be
 * deleted or non-zero if the keepalive connection should not be deleted. */
static int
do_keepalive_connection_callback(struct keepalive_connection *keep_conn)
{
	struct uri *proxied_uri = get_proxied_uri(keep_conn->uri);
	struct uri *proxy_uri   = get_proxy_uri(keep_conn->uri, NULL);

	if (proxied_uri && proxy_uri) {
		struct connection *conn;

		conn = init_connection(proxy_uri, proxied_uri, NULL, 0,
				       CACHE_MODE_NEVER, PRI_CANCEL);

		if (conn) {
			void (*done)(struct connection *) = keep_conn->done;

			add_to_queue(conn);

			/* Get the keepalive info and let it clean up */
			if (!has_keepalive_connection(conn)
			    || !add_host_connection(conn)) {
				free_connection_data(conn);
				done_connection(conn);
				return 0;
			}

			active_connections++;
			conn->running = 1;
			done(conn);
			return 1;
		}
	}

	if (proxied_uri) done_uri(proxied_uri);
	if (proxy_uri) done_uri(proxy_uri);

	return 0;
}

static inline void
done_keepalive_connection(struct keepalive_connection *keep_conn)
{
	if (keep_conn->done && do_keepalive_connection_callback(keep_conn))
		return;

	del_from_list(keep_conn);
	if (keep_conn->socket != -1) close(keep_conn->socket);
	done_uri(keep_conn->uri);
	mem_free(keep_conn);
}

static struct keepalive_connection *
init_keepalive_connection(struct connection *conn, long timeout_in_seconds,
			  void (*done)(struct connection *))
{
	struct keepalive_connection *keep_conn;
	struct uri *uri = conn->uri;

	assert(uri->host);
	if_assert_failed return NULL;

	keep_conn = mem_calloc(1, sizeof(*keep_conn));
	if (!keep_conn) return NULL;

	keep_conn->uri = get_uri_reference(uri);
	keep_conn->done = done;
	keep_conn->protocol_family = conn->socket->protocol_family;
	keep_conn->socket = conn->socket->fd;
	timeval_from_seconds(&keep_conn->timeout, timeout_in_seconds);
	timeval_now(&keep_conn->creation_time);

	return keep_conn;
}

static struct keepalive_connection *
get_keepalive_connection(struct connection *conn)
{
	struct keepalive_connection *keep_conn;

	if (!conn->uri->host) return NULL;

	foreach (keep_conn, keepalive_connections)
		if (compare_uri(keep_conn->uri, conn->uri, URI_KEEPALIVE))
			return keep_conn;

	return NULL;
}

int
has_keepalive_connection(struct connection *conn)
{
	struct keepalive_connection *keep_conn = get_keepalive_connection(conn);

	if (!keep_conn) return 0;

	conn->socket->fd = keep_conn->socket;
	conn->socket->protocol_family = keep_conn->protocol_family;

	/* Mark that the socket should not be closed and the callback should be
	 * ignored. */
	keep_conn->socket = -1;
	keep_conn->done = NULL;
	done_keepalive_connection(keep_conn);

	return 1;
}

void
add_keepalive_connection(struct connection *conn, long timeout_in_seconds,
			 void (*done)(struct connection *))
{
	struct keepalive_connection *keep_conn;

	assertm(conn->socket->fd != -1, "keepalive connection not connected");
	if_assert_failed goto done;

	keep_conn = init_keepalive_connection(conn, timeout_in_seconds, done);
	if (keep_conn) {
		/* Make sure that the socket descriptor will not periodically be
		 * checked or closed by free_connection_data(). */
		clear_handlers(conn->socket->fd);
		conn->socket->fd = -1;
		add_to_list(keepalive_connections, keep_conn);

	} else if (done) {
		/* It will take just a little more time */
		done(conn);
		return;
	}

done:
	free_connection_data(conn);
	done_connection(conn);
	register_check_queue();
}

/* Timer callback for @keepalive_timeout.  As explained in @install_timer,
 * this function must erase the expired timer ID from all variables.  */
static void
keepalive_timer(void *x)
{
	keepalive_timeout = TIMER_ID_UNDEF;
	/* The expired timer ID has now been erased.  */
	check_keepalive_connections();
}

void
check_keepalive_connections(void)
{
	struct keepalive_connection *keep_conn, *next;
	timeval_T now;
	int p = 0;

	timeval_now(&now);

	kill_timer(&keepalive_timeout);

	foreachsafe (keep_conn, next, keepalive_connections) {
		timeval_T age;

		if (can_read(keep_conn->socket)) {
			done_keepalive_connection(keep_conn);
			continue;
		}

		timeval_sub(&age, &keep_conn->creation_time, &now);
		if (timeval_cmp(&age, &keep_conn->timeout) > 0) {
			done_keepalive_connection(keep_conn);
			continue;
		}

		p++;
	}

	for (; p > MAX_KEEPALIVE_CONNECTIONS; p--) {
		assertm(!list_empty(keepalive_connections), "keepalive list empty");
		if_assert_failed return;
		done_keepalive_connection(keepalive_connections.prev);
	}

	if (!list_empty(keepalive_connections))
		install_timer(&keepalive_timeout, KEEPALIVE_CHECK_TIME,
			      keepalive_timer, NULL);
}

static inline void
abort_all_keepalive_connections(void)
{
	while (!list_empty(keepalive_connections))
		done_keepalive_connection(keepalive_connections.next);

	check_keepalive_connections();
}


static void
sort_queue(void)
{
	while (1) {
		struct connection *conn;
		int swp = 0;

		foreach (conn, connection_queue) {
			if (!list_has_next(connection_queue, conn)) break;

			if (get_priority(conn->next) < get_priority(conn)) {
				struct connection *c = conn->next;

				del_from_list(conn);
				add_at_pos(c, conn);
				swp = 1;
			}
		}

		if (!swp) break;
	};
}

static void
interrupt_connection(struct connection *conn)
{
	free_connection_data(conn);
}

static inline void
suspend_connection(struct connection *conn)
{
	interrupt_connection(conn);
	set_connection_state(conn, connection_state(S_WAIT));
}

static void
run_connection(struct connection *conn)
{
	protocol_handler_T *func = get_protocol_handler(conn->uri->protocol);

	assert(func);

	assertm(!conn->running, "connection already running");
	if_assert_failed return;

	if (!add_host_connection(conn)) {
		set_connection_state(conn, connection_state(S_OUT_OF_MEM));
		done_connection(conn);
		return;
	}

	active_connections++;
	conn->running = 1;

	func(conn);
}

/* Set certain state on a connection and then abort the connection. */
void
abort_connection(struct connection *conn, struct connection_state state)
{
	assertm(is_in_result_state(state),
		"connection didn't end in result state (%d)", state);

	if (is_in_state(state, S_OK) && conn->cached)
		normalize_cache_entry(conn->cached, conn->from);

	set_connection_state(conn, state);

	if (conn->running) interrupt_connection(conn);
	done_connection(conn);
	register_check_queue();
}

/* Set certain state on a connection and then retry the connection. */
void
retry_connection(struct connection *conn, struct connection_state state)
{
	int max_tries = get_opt_int("connection.retries");

	assertm(is_in_result_state(state),
		"connection didn't end in result state (%d)", state);

	set_connection_state(conn, state);

	interrupt_connection(conn);
	if (conn->uri->post || (max_tries && ++conn->tries >= max_tries)) {
		done_connection(conn);
		register_check_queue();
	} else {
		conn->prev_error = conn->state;
		run_connection(conn);
	}
}

static int
try_to_suspend_connection(struct connection *conn, struct uri *uri)
{
	enum connection_priority priority = get_priority(conn);
	struct connection *c;

	foreachback (c, connection_queue) {
		if (get_priority(c) <= priority) return -1;
		if (is_in_state(c->state, S_WAIT)) continue;
		if (c->uri->post && get_priority(c) < PRI_CANCEL) continue;
		if (uri && !compare_uri(uri, c->uri, URI_HOST)) continue;
		suspend_connection(c);
		return 0;
	}

	return -1;
}

static inline int
try_connection(struct connection *conn, int max_conns_to_host, int max_conns)
{
	struct host_connection *host_conn = get_host_connection(conn);

	if (host_conn && get_object_refcount(host_conn) >= max_conns_to_host)
		return try_to_suspend_connection(conn, host_conn->uri) ? 0 : -1;

	if (active_connections >= max_conns)
		return try_to_suspend_connection(conn, NULL) ? 0 : -1;

	run_connection(conn);
	return 1;
}

static void
check_queue(void)
{
	struct connection *conn;
	int max_conns_to_host = get_opt_int("connection.max_connections_to_host");
	int max_conns = get_opt_int("connection.max_connections");

again:
	conn = connection_queue.next;
	check_queue_bugs();
	check_keepalive_connections();

	while (conn != (struct connection *) &connection_queue) {
		struct connection *c;
		enum connection_priority pri = get_priority(conn);

		for (c = conn; c != (struct connection *) &connection_queue && get_priority(c) == pri;) {
			struct connection *cc = c;

			c = c->next;
			if (is_in_state(cc->state, S_WAIT)
			    && get_keepalive_connection(cc)
			    && try_connection(cc, max_conns_to_host, max_conns))
				goto again;
		}

		for (c = conn; c != (struct connection *) &connection_queue && get_priority(c) == pri;) {
			struct connection *cc = c;

			c = c->next;
			if (is_in_state(cc->state, S_WAIT)
			    && try_connection(cc, max_conns_to_host, max_conns))
				goto again;
		}
		conn = c;
	}

again2:
	foreachback (conn, connection_queue) {
		if (get_priority(conn) < PRI_CANCEL) break;
		if (is_in_state(conn->state, S_WAIT)) {
			set_connection_state(conn, connection_state(S_INTERRUPTED));
			done_connection(conn);
			goto again2;
		}
	}

	check_queue_bugs();
}

int
register_check_queue(void)
{
	return register_bottom_half(check_queue, NULL);
}

int
load_uri(struct uri *uri, struct uri *referrer, struct download *download,
	 enum connection_priority pri, enum cache_mode cache_mode, off_t start)
{
	struct cache_entry *cached;
	struct connection *conn;
	struct uri *proxy_uri, *proxied_uri;
	struct connection_state error_state = connection_state(S_OK);

	if (download) {
		download->conn = NULL;
		download->cached = NULL;
		download->pri = pri;
		download->state = connection_state(S_OUT_OF_MEM);
		download->prev_error = connection_state(0);
	}

#ifdef CONFIG_DEBUG
	foreach (conn, connection_queue) {
		struct download *assigned;

		foreach (assigned, conn->downloads) {
			assertm(assigned != download, "Download assigned to '%s'", struri(conn->uri));
			if_assert_failed {
				download->state = connection_state(S_INTERNAL);
				if (download->callback)
					download->callback(download, download->data);
				return 0;
			}
			/* No recovery path should be necessary. */
		}
	}
#endif

	cached = get_validated_cache_entry(uri, cache_mode);
	if (cached) {
		if (download) {
			download->cached = cached;
			download->state = connection_state(S_OK);
			/* XXX:
			 * This doesn't work since sometimes |download->progress|
			 * is undefined and contains random memory locations.
			 * It's not supposed to point on anything here since
			 * |download| has no connection attached.
			 * Downloads resuming will probably break in some
			 * cases without this, though.
			 * FIXME: Needs more investigation. --pasky */
			/* if (download->progress) download->progress->start = start; */
			if (download->callback)
				download->callback(download, download->data);
		}
		return 0;
	}

	proxied_uri = get_proxied_uri(uri);
	proxy_uri   = get_proxy_uri(uri, &error_state);

	if (!proxy_uri
	    || !proxied_uri
	    || (get_protocol_need_slash_after_host(proxy_uri->protocol)
		&& !proxy_uri->hostlen)) {

		if (download) {
			if (is_in_state(error_state, S_OK)) {
				error_state = proxy_uri && proxied_uri
					? connection_state(S_BAD_URL)
					: connection_state(S_OUT_OF_MEM);
			}

			download->state = error_state;
			download->callback(download, download->data);
		}
		if (proxy_uri) done_uri(proxy_uri);
		if (proxied_uri) done_uri(proxied_uri);
		return -1;
	}

	foreach (conn, connection_queue) {
		if (conn->detached
		    || !compare_uri(conn->uri, proxy_uri, 0))
			continue;

		done_uri(proxy_uri);
		done_uri(proxied_uri);

		if (get_priority(conn) > pri) {
			del_from_list(conn);
			conn->pri[pri]++;
			add_to_queue(conn);
			register_check_queue();
		} else {
			conn->pri[pri]++;
		}

		if (download) {
			download->progress = conn->progress;
			download->conn = conn;
			download->cached = conn->cached;
			add_to_list(conn->downloads, download);
			/* This is likely to call download->callback() now! */
			set_connection_state(conn, conn->state);
		}
		check_queue_bugs();
		return 0;
	}

	conn = init_connection(proxy_uri, proxied_uri, referrer, start, cache_mode, pri);
	if (!conn) {
		if (download) {
			download->state = connection_state(S_OUT_OF_MEM);
			download->callback(download, download->data);
		}
		if (proxy_uri) done_uri(proxy_uri);
		if (proxied_uri) done_uri(proxied_uri);
		return -1;
	}

	if (cache_mode < CACHE_MODE_FORCE_RELOAD && cached && !list_empty(cached->frag)
	    && !((struct fragment *) cached->frag.next)->offset)
		conn->from = ((struct fragment *) cached->frag.next)->length;

	if (download) {
		download->progress = conn->progress;
		download->conn = conn;
		download->cached = NULL;
		download->state = connection_state(S_OK);
		add_to_list(conn->downloads, download);
	}

	add_to_queue(conn);
	set_connection_state(conn, connection_state(S_WAIT));

	check_queue_bugs();

	register_check_queue();
	return 0;
}


/* FIXME: one object in more connections */
void
cancel_download(struct download *download, int interrupt)
{
	struct connection *conn;

	assert(download);
	if_assert_failed return;

	/* Did the connection already end? */
	if (is_in_result_state(download->state))
		return;

	assertm(download->conn != NULL, "last state is %d", download->state);

	check_queue_bugs();

	download->state = connection_state(S_INTERRUPTED);
	del_from_list(download);

	conn = download->conn;

	conn->pri[download->pri]--;
	assertm(conn->pri[download->pri] >= 0, "priority counter underflow");
	if_assert_failed conn->pri[download->pri] = 0;

	if (list_empty(conn->downloads)) {
		/* Necessary because of assertion in get_priority(). */
		conn->pri[PRI_CANCEL]++;

		if (conn->detached || interrupt)
			abort_connection(conn, connection_state(S_INTERRUPTED));
	}

	sort_queue();
	check_queue_bugs();

	register_check_queue();
}

void
move_download(struct download *old, struct download *new,
	      enum connection_priority newpri)
{
	struct connection *conn;

	assert(old);

	/* The download doesn't necessarily have a connection attached, for
	 * example the file protocol loads it's object immediately. This is
	 * catched by the result state check below. */

	conn = old->conn;

	new->conn	= conn;
	new->cached	= old->cached;
	new->prev_error	= old->prev_error;
	new->progress	= old->progress;
	new->state	= old->state;
	new->pri	= newpri;

	if (is_in_result_state(old->state)) {
		/* Ensure that new->conn is always "valid", that is NULL if the
		 * connection has been detached and non-NULL otherwise. */
		if (new->callback) {
			new->conn = NULL;
			new->progress = NULL;
			new->callback(new, new->data);
		}
		return;
	}

	assertm(old->conn != NULL, "last state is %d", old->state);

	conn->pri[new->pri]++;
	add_to_list(conn->downloads, new);
	/* In principle, we need to sort_queue() only if conn->pri[new->pri]
	 * just changed from 0 to 1.  But the risk of bugs is smaller if we
	 * sort every time.  */
	sort_queue();

	cancel_download(old, 0);
}


/* This will remove 'pos' bytes from the start of the cache for the specified
 * connection, if the cached object is already too big. */
void
detach_connection(struct download *download, off_t pos)
{
	struct connection *conn = download->conn;

	if (is_in_result_state(download->state)) return;

	if (!conn->detached) {
		off_t total_len;
		off_t i, total_pri = 0;

		if (!conn->cached)
			return;

		total_len = (conn->est_length == -1) ? conn->from
						     : conn->est_length;

		if (total_len < (get_opt_long("document.cache.memory.size")
				 * MAX_CACHED_OBJECT_PERCENT / 100)) {
			/* This whole thing will fit to the memory anyway, so
			 * there's no problem in detaching the connection. */
			return;
		}

		for (i = 0; i < PRI_CANCEL; i++)
			total_pri += conn->pri[i];
		assertm(total_pri, "detaching free connection");
		/* No recovery path should be necessary...? */

		/* Pre-clean cache. */
		shrink_format_cache(0);

		if (total_pri != 1 || is_object_used(conn->cached)) {
			/* We're too important, or someone uses our cache
			 * entry. */
			return;
		}

		/* DBG("detached"); */

		/* We aren't valid cache entry anymore. */
		conn->cached->valid = 0;
		conn->detached = 1;
	}

	/* Strip the entry. */
	free_entry_to(conn->cached, pos);
}

/* Timer callback for @conn->timer.  As explained in @install_timer,
 * this function must erase the expired timer ID from all variables.  */
static void
connection_timeout(struct connection *conn)
{
	conn->timer = TIMER_ID_UNDEF;
	/* The expired timer ID has now been erased.  */
	timeout_socket(conn->socket);
}

/* Timer callback for @conn->timer.  As explained in @install_timer,
 * this function must erase the expired timer ID from all variables.
 *
 * Huh, using two timers? Is this to account for changes of c->unrestartable
 * or can it be reduced? --jonas */
static void
connection_timeout_1(struct connection *conn)
{
	install_timer(&conn->timer, (milliseconds_T)
			((conn->unrestartable
			 ? get_opt_int("connection.unrestartable_receive_timeout")
			 : get_opt_int("connection.receive_timeout"))
			* 500), (void (*)(void *)) connection_timeout, conn);
	/* The expired timer ID has now been erased.  */
}

void
set_connection_timeout(struct connection *conn)
{
	kill_timer(&conn->timer);

	install_timer(&conn->timer, (milliseconds_T)
			((conn->unrestartable
			 ? get_opt_int("connection.unrestartable_receive_timeout")
			 : get_opt_int("connection.receive_timeout"))
			* 500), (void (*)(void *)) connection_timeout_1, conn);
}


void
abort_all_connections(void)
{
	while (!list_empty(connection_queue)) {
		abort_connection(connection_queue.next,
				 connection_state(S_INTERRUPTED));
	}

	abort_all_keepalive_connections();
}

void
abort_background_connections(void)
{
	struct connection *conn, *next;

	foreachsafe (conn, next, connection_queue) {
		if (get_priority(conn) >= PRI_CANCEL)
			abort_connection(conn, connection_state(S_INTERRUPTED));
	}
}

int
is_entry_used(struct cache_entry *cached)
{
	struct connection *conn;

	foreach (conn, connection_queue)
		if (conn->cached == cached)
			return 1;

	return 0;
}
