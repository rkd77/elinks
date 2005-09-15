/* Connections management */
/* $Id: connection.c,v 1.210.4.6 2005/05/01 21:15:33 jonas Exp $ */

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
#include "lowlevel/connect.h"
#include "lowlevel/dns.h"
#include "lowlevel/select.h"
#include "protocol/protocol.h"
#include "protocol/proxy.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "sched/session.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/string.h"
#include "util/ttime.h"
#include "util/types.h"


struct keepalive_connection {
	LIST_HEAD(struct keepalive_connection);

	/* XXX: This is just the URI of the connection that registered the
	 * keepalive connection so only rely on the protocol, user, password,
	 * host and port part. */
	struct uri *uri;

	/* Function called when the keepalive has timed out or is deleted */
	void (*done)(struct connection *);

	ttime timeout;
	ttime add_time;

	unsigned int protocol_family:1; /* 0 == PF_INET, 1 == PF_INET6 */
	int socket;
};


static unsigned int connection_id = 0;
static int active_connections = 0;
static int keepalive_timeout = -1;

/* TODO: queue probably shouldn't be exported; ideally we should probably
 * separate it to an own module and define operations on it (especially
 * foreach_queue or so). Ok ok, that's nothing important and I'm not even
 * sure I would really like it ;-). --pasky */
INIT_LIST_HEAD(queue);
static INIT_LIST_HEAD(host_connections);
static INIT_LIST_HEAD(keepalive_connections);

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

long
connect_info(int type)
{
	long info = 0;
	struct connection *conn;
	struct keepalive_connection *keep_conn;

	switch (type) {
		case INFO_FILES:
			foreach (conn, queue) info++;
			break;
		case INFO_CONNECTING:
			foreach (conn, queue)
				info += is_in_connecting_state(conn->state);
			break;
		case INFO_TRANSFER:
			foreach (conn, queue)
				info += is_in_transfering_state(conn->state);
			break;
		case INFO_KEEP:
			foreach (keep_conn, keepalive_connections) info++;
			break;
	}

	return info;
}

static inline int
connection_disappeared(struct connection *conn)
{
	struct connection *c;

	foreach (c, queue)
		if (conn == c && conn->id == c->id)
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
	LIST_HEAD(struct host_connection);

	/* XXX: This is just the URI of the connection that registered the
	 * host connection so only rely on the host part. */
	struct uri *uri;
	struct object object;
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

	foreach (conn, queue) {
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


static struct connection *
init_connection(struct uri *uri, struct uri *proxied_uri, struct uri *referrer,
		int start, enum cache_mode cache_mode,
		enum connection_priority priority)
{
	struct connection *conn = mem_calloc(1, sizeof(*conn));

	if (!conn) return NULL;

	assert(proxied_uri->protocol != PROTOCOL_PROXY);

	/* load_uri() gets the URI from get_proxy() which grabs a reference for
	 * us. */
	conn->uri = uri;
	conn->proxied_uri = proxied_uri;
	conn->id = connection_id++;
	conn->pri[priority] = 1;
	conn->cache_mode = cache_mode;
	conn->socket.fd = conn->data_socket.fd = -1;
	conn->content_encoding = ENCODING_NONE;
	conn->stream_pipes[0] = conn->stream_pipes[1] = -1;
	conn->cgi_pipes[0] = conn->cgi_pipes[1] = -1;
	init_list(conn->downloads);
	conn->est_length = -1;
	conn->progress.start = start;
	conn->progress.timer = -1;
	conn->timer = -1;

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

static void stat_timer(struct connection *conn);

static void
update_progress(struct connection *conn)
{
	struct progress *progress = &conn->progress;
	ttime a = get_time() - progress->last_time;

	progress->loaded = conn->received;
	progress->size = conn->est_length;
	progress->pos = conn->from;
	if (progress->size < progress->pos && progress->size != -1)
		progress->size = conn->from;

	progress->dis_b += a;
	while (progress->dis_b >= SPD_DISP_TIME * CURRENT_SPD_SEC) {
		progress->cur_loaded -= progress->data_in_secs[0];
		memmove(progress->data_in_secs, progress->data_in_secs + 1,
			sizeof(*progress->data_in_secs) * (CURRENT_SPD_SEC - 1));
		progress->data_in_secs[CURRENT_SPD_SEC - 1] = 0;
		progress->dis_b -= SPD_DISP_TIME;
	}

	progress->data_in_secs[CURRENT_SPD_SEC - 1] += progress->loaded - progress->last_loaded;
	progress->cur_loaded += progress->loaded - progress->last_loaded;
	progress->last_loaded = progress->loaded;
	progress->last_time += a;
	progress->elapsed += a;
	progress->timer = install_timer(SPD_DISP_TIME, (void (*)(void *)) stat_timer, conn);
}

static void
stat_timer(struct connection *conn)
{
	update_progress(conn);
	notify_connection_callbacks(conn);
}

void
set_connection_state(struct connection *conn, enum connection_state state)
{
	struct download *download;
	struct progress *progress = &conn->progress;

	if (is_in_result_state(conn->state) && is_in_progress_state(state))
		conn->prev_error = conn->state;

	conn->state = state;
	if (conn->state == S_TRANS) {
		if (progress->timer == -1) {
			if (!progress->valid) {
				int tmp = progress->start;
				int tmp2 = progress->seek;

				memset(progress, 0, sizeof(*progress));
				progress->start = tmp;
				progress->seek = tmp2;
				progress->valid = 1;
			}
			progress->last_time = get_time();
			progress->last_loaded = progress->loaded;
			update_progress(conn);
			if (connection_disappeared(conn))
				return;
		}

	} else if (progress->timer != -1) {
		kill_timer(progress->timer);
		progress->timer = -1;
	}

	foreach (download, conn->downloads) {
		download->state = state;
		download->prev_error = conn->prev_error;
	}

	if (is_in_progress_state(state)) notify_connection_callbacks(conn);
}

static void
free_connection_data(struct connection *conn)
{
	assertm(conn->running, "connection already suspended");
	/* XXX: Recovery path? Originally, there was none. I think we'll get
	 * at least active_connections underflows along the way. --pasky */
	conn->running = 0;

	if (conn->socket.fd != -1)
		clear_handlers(conn->socket.fd);
	if (conn->data_socket.fd != -1)
		clear_handlers(conn->data_socket.fd);
	close_socket(NULL, &conn->data_socket);

	/* XXX: See also protocol/http/http.c:decompress_shutdown(). */
	if (conn->stream) {
		close_encoded(conn->stream);
		conn->stream = NULL;
	}
	if (conn->stream_pipes[1] >= 0)
		close(conn->stream_pipes[1]);
	conn->stream_pipes[0] = conn->stream_pipes[1] = -1;

	if (conn->cgi_pipes[0] >= 0)
		close(conn->cgi_pipes[0]);
	if (conn->cgi_pipes[1] >= 0)
		close(conn->cgi_pipes[1]);
	conn->cgi_pipes[0] = conn->cgi_pipes[1] = -1;

	if (conn->dnsquery) {
		kill_dns_request(&conn->dnsquery);
	}
	if (conn->conn_info) {
		/* No callbacks should be made */
		conn->conn_info->done = NULL;
		done_connection_info(conn);
	}

	mem_free_set(&conn->buffer, NULL);
	mem_free_set(&conn->info, NULL);

	if (conn->timer != -1) {
		kill_timer(conn->timer);
		conn->timer = -1;
	}

	active_connections--;
	assertm(active_connections >= 0, "active connections underflow");
	if_assert_failed active_connections = 0;

	if (conn->state != S_WAIT)
		done_host_connection(conn);
}

void
notify_connection_callbacks(struct connection *conn)
{
	enum connection_state state = conn->state;
	struct download *download, *next;

	foreachsafe (download, next, conn->downloads) {
		download->cached = conn->cached;
		if (download->callback)
			download->callback(download, download->data);
		if (is_in_progress_state(state) && connection_disappeared(conn))
			return;
	}
}

static void
done_connection(struct connection *conn)
{
	if (!is_in_result_state(conn->state))
		set_connection_state(conn, S_INTERNAL);
	del_from_list(conn);
	notify_connection_callbacks(conn);
	if (conn->referrer) done_uri(conn->referrer);
	done_uri(conn->uri);
	done_uri(conn->proxied_uri);
	mem_free(conn);
	check_queue_bugs();
}

static inline void add_to_queue(struct connection *conn);

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
init_keepalive_connection(struct connection *conn, ttime timeout,
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
	keep_conn->protocol_family = conn->protocol_family;
	keep_conn->socket = conn->socket.fd;
	keep_conn->timeout = timeout;
	keep_conn->add_time = get_time();

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

	conn->socket.fd = keep_conn->socket;
	conn->protocol_family = keep_conn->protocol_family;

	/* Mark that the socket should not be closed and the callback should be
	 * ignored. */
	keep_conn->socket = -1;
	keep_conn->done = NULL;
	done_keepalive_connection(keep_conn);

	return 1;
}

void
add_keepalive_connection(struct connection *conn, ttime timeout,
			 void (*done)(struct connection *))
{
	struct keepalive_connection *keep_conn;

	assertm(conn->socket.fd != -1, "keepalive connection not connected");
	if_assert_failed goto done;

	keep_conn = init_keepalive_connection(conn, timeout, done);
	if (keep_conn) {
		add_to_list(keepalive_connections, keep_conn);

	} else if (done) {
		/* It will take just a little more time */
		done(conn);
		return;

	} else {
		close(conn->socket.fd);
	}

done:
	free_connection_data(conn);
	done_connection(conn);
	register_bottom_half((void (*)(void *)) check_queue, NULL);
}

static void
keepalive_timer(void *x)
{
	keepalive_timeout = -1;
	check_keepalive_connections();
}

void
check_keepalive_connections(void)
{
	struct keepalive_connection *keep_conn, *next;
	ttime ct = get_time();
	int p = 0;

	if (keepalive_timeout != -1) {
		kill_timer(keepalive_timeout);
		keepalive_timeout = -1;
	}

	foreachsafe (keep_conn, next, keepalive_connections) {
		if (can_read(keep_conn->socket)
		    || ct - keep_conn->add_time > keep_conn->timeout) {
			done_keepalive_connection(keep_conn);
		} else {
			p++;
		}
	}

	for (; p > MAX_KEEPALIVE_CONNECTIONS; p--) {
		assertm(!list_empty(keepalive_connections), "keepalive list empty");
		if_assert_failed return;
		done_keepalive_connection(keepalive_connections.prev);
	}

	if (!list_empty(keepalive_connections))
		keepalive_timeout = install_timer(KEEPALIVE_CHECK_TIME,
						  keepalive_timer, NULL);
}

static inline void
abort_all_keepalive_connections(void)
{
	while (!list_empty(keepalive_connections))
		done_keepalive_connection(keepalive_connections.next);

	check_keepalive_connections();
}


static inline void
add_to_queue(struct connection *conn)
{
	struct connection *c;
	enum connection_priority priority = get_priority(conn);

	foreach (c, queue)
		if (get_priority(c) > priority)
			break;

	add_at_pos(c->prev, conn);
}

static void
sort_queue(void)
{
	int swp;

	do {
		struct connection *conn;

		swp = 0;
		foreach (conn, queue) {
			if (!list_has_next(queue, conn)) break;

			if (get_priority(conn->next) < get_priority(conn)) {
				struct connection *c = conn->next;

				del_from_list(conn);
				add_at_pos(c, conn);
				swp = 1;
			}
		}
	} while (swp);
}

static void
interrupt_connection(struct connection *conn)
{
	close_socket(conn, &conn->socket);
	free_connection_data(conn);
}

static inline void
suspend_connection(struct connection *conn)
{
	interrupt_connection(conn);
	set_connection_state(conn, S_WAIT);
}

static void
run_connection(struct connection *conn)
{
	protocol_handler *func = get_protocol_handler(conn->uri->protocol);

	assert(func);

	assertm(!conn->running, "connection already running");
	if_assert_failed return;

	if (!add_host_connection(conn)) {
		set_connection_state(conn, S_OUT_OF_MEM);
		done_connection(conn);
		return;
	}

	active_connections++;
	conn->running = 1;

	func(conn);
}

void
retry_connection(struct connection *conn)
{
	int max_tries = get_opt_int("connection.retries");

	interrupt_connection(conn);
	if (conn->uri->post || !max_tries || ++conn->tries >= max_tries) {
		/* notify_connection_callbacks(conn); */
		done_connection(conn);
		register_bottom_half((void (*)(void *)) check_queue, NULL);
	} else {
		conn->prev_error = conn->state;
		run_connection(conn);
	}
}

void
abort_connection(struct connection *conn)
{
	if (conn->running) interrupt_connection(conn);
	/* notify_connection_callbacks(conn); */
	done_connection(conn);
	register_bottom_half((void (*)(void *)) check_queue, NULL);
}

/* Set certain state on a connection and then abort the connection. */
void
abort_conn_with_state(struct connection *conn, enum connection_state state)
{
	set_connection_state(conn, state);
	abort_connection(conn);
}

/* Set certain state on a connection and then retry the connection. */
void
retry_conn_with_state(struct connection *conn, enum connection_state state)
{
	set_connection_state(conn, state);
	retry_connection(conn);
}

static int
try_to_suspend_connection(struct connection *conn, struct uri *uri)
{
	enum connection_priority priority = get_priority(conn);
	struct connection *c;

	foreachback (c, queue) {
		if (get_priority(c) <= priority) return -1;
		if (c->state == S_WAIT) continue;
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

void
check_queue(void)
{
	struct connection *conn;
	int max_conns_to_host = get_opt_int("connection.max_connections_to_host");
	int max_conns = get_opt_int("connection.max_connections");

again:
	conn = queue.next;
	check_queue_bugs();
	check_keepalive_connections();

	while (conn != (struct connection *) &queue) {
		struct connection *c;
		enum connection_priority pri = get_priority(conn);

		for (c = conn; c != (struct connection *) &queue && get_priority(c) == pri;) {
			struct connection *cc = c;

			c = c->next;
			if (cc->state == S_WAIT && get_keepalive_connection(cc)
			    && try_connection(cc, max_conns_to_host, max_conns))
				goto again;
		}

		for (c = conn; c != (struct connection *) &queue && get_priority(c) == pri;) {
			struct connection *cc = c;

			c = c->next;
			if (cc->state == S_WAIT
			    && try_connection(cc, max_conns_to_host, max_conns))
				goto again;
		}
		conn = c;
	}

again2:
	foreachback (conn, queue) {
		if (get_priority(conn) < PRI_CANCEL) break;
		if (conn->state == S_WAIT) {
			set_connection_state(conn, S_INTERRUPTED);
			done_connection(conn);
			goto again2;
		}
	}

	check_queue_bugs();
}

int
load_uri(struct uri *uri, struct uri *referrer, struct download *download,
	 enum connection_priority pri, enum cache_mode cache_mode, int start)
{
	struct cache_entry *cached;
	struct connection *conn;
	struct uri *proxy_uri, *proxied_uri;
	enum connection_state connection_state = S_OK;

	if (download) {
		download->conn = NULL;
		download->cached = NULL;
		download->pri = pri;
		download->state = S_OUT_OF_MEM;
		download->prev_error = 0;
	}

#ifdef CONFIG_DEBUG
	foreach (conn, queue) {
		struct download *assigned;

		foreach (assigned, conn->downloads) {
			assertm(assigned != download, "Download assigned to '%s'", struri(conn->uri));
			if_assert_failed {
				download->state = S_INTERNAL;
				if (download->callback) download->callback(download, download->data);
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
			download->state = S_OK;
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
	proxy_uri   = get_proxy_uri(uri, &connection_state);

	if (!proxy_uri
	    || !proxied_uri
	    || (get_protocol_need_slash_after_host(proxy_uri->protocol)
		&& !proxy_uri->hostlen)) {

		if (download) {
			if (connection_state == S_OK) {
				connection_state = proxy_uri && proxied_uri
						 ? S_BAD_URL : S_OUT_OF_MEM;
			}

			download->state = connection_state;
			download->callback(download, download->data);
		}
		if (proxy_uri) done_uri(proxy_uri);
		if (proxied_uri) done_uri(proxied_uri);
		return -1;
	}

	foreach (conn, queue) {
		if (conn->detached
		    || !compare_uri(conn->uri, proxy_uri, 0))
			continue;

		done_uri(proxy_uri);
		done_uri(proxied_uri);

		if (get_priority(conn) > pri) {
			del_from_list(conn);
			conn->pri[pri]++;
			add_to_queue(conn);
			register_bottom_half((void (*)(void *)) check_queue, NULL);
		} else {
			conn->pri[pri]++;
		}

		if (download) {
			download->progress = &conn->progress;
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
			download->state = S_OUT_OF_MEM;
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
		download->progress = &conn->progress;
		download->conn = conn;
		download->cached = NULL;
		add_to_list(conn->downloads, download);
	}

	add_to_queue(conn);
	set_connection_state(conn, S_WAIT);

	check_queue_bugs();

	register_bottom_half((void (*)(void *)) check_queue, NULL);
	return 0;
}


/* FIXME: one object in more connections */
void
change_connection(struct download *old, struct download *new,
		  int newpri, int interrupt)
{
	struct connection *conn;

	assert(old);
	if_assert_failed return;

	if (is_in_result_state(old->state)) {
		if (new) {
			new->cached = old->cached;
			new->state = old->state;
			new->prev_error = old->prev_error;
			if (new->callback) new->callback(new, new->data);
		}
		return;
	}

	check_queue_bugs();

	conn = old->conn;

	conn->pri[old->pri]--;
	assertm(conn->pri[old->pri] >= 0, "priority counter underflow");
	if_assert_failed conn->pri[old->pri] = 0;

	conn->pri[newpri]++;
	del_from_list(old);
	old->state = S_INTERRUPTED;

	if (new) {
		new->progress = &conn->progress;
		add_to_list(conn->downloads, new);
		new->state = conn->state;
		new->prev_error = conn->prev_error;
		new->pri = newpri;
		new->conn = conn;
		new->cached = conn->cached;

	} else if (conn->detached || interrupt) {
		abort_conn_with_state(conn, S_INTERRUPTED);
	}

	sort_queue();
	check_queue_bugs();

	register_bottom_half((void (*)(void *)) check_queue, NULL);
}

/* This will remove 'pos' bytes from the start of the cache for the specified
 * connection, if the cached object is already too big. */
void
detach_connection(struct download *download, int pos)
{
	struct connection *conn = download->conn;

	if (is_in_result_state(download->state)) return;

	if (!conn->detached) {
		int total_len;
		int i, total_pri = 0;

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

static void
connection_timeout(struct connection *conn)
{
	conn->timer = -1;
	set_connection_state(conn, S_TIMEOUT);
	if (conn->dnsquery) {
		abort_connection(conn);
	} else if (conn->conn_info) {
		dns_found(conn, 0); /* jump to next addr */
		if (conn->conn_info) set_connection_timeout(conn);
	} else {
		retry_connection(conn);
	}
}

/* Huh, using two timers? Is this to account for changes of c->unrestartable
 * or can it be reduced? --jonas */
static void
connection_timeout_1(struct connection *conn)
{
	conn->timer = install_timer((conn->unrestartable
				     ? get_opt_int("connection.unrestartable_receive_timeout")
				     : get_opt_int("connection.receive_timeout"))
				    * 500, (void (*)(void *)) connection_timeout, conn);
}

void
set_connection_timeout(struct connection *conn)
{
	if (conn->timer != -1) kill_timer(conn->timer);
	conn->timer = install_timer((conn->unrestartable
				     ? get_opt_int("connection.unrestartable_receive_timeout")
				     : get_opt_int("connection.receive_timeout"))
				    * 500, (void (*)(void *)) connection_timeout_1, conn);
}


void
abort_all_connections(void)
{
	while (!list_empty(queue)) {
		abort_conn_with_state(queue.next, S_INTERRUPTED);
	}

	abort_all_keepalive_connections();
}

void
abort_background_connections(void)
{
	struct connection *conn;

	foreach (conn, queue) {
		if (get_priority(conn) >= PRI_CANCEL) {
			conn = conn->prev;
			abort_conn_with_state(conn->next, S_INTERRUPTED);
		}
	}
}
