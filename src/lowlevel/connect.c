/* Sockets-o-matic */
/* $Id: connect.c,v 1.114.2.6 2005/05/01 21:18:43 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_GETIFADDRS
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>		/* getifaddrs() */
#endif
#endif				/* HAVE_GETIFADDRS */

#include "elinks.h"

#include "config/options.h"
#include "lowlevel/connect.h"
#include "lowlevel/dns.h"
#include "lowlevel/select.h"
#include "osdep/osdep.h"
#include "osdep/getifaddrs.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "ssl/connect.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

/* To enable logging of tranfers, for debugging purposes. */
#if 0

#define DEBUG_TRANSFER_LOGFILE "/tmp/log"

static void
debug_transfer_log(unsigned char *data, int len)
{
	int fd = open(DEBUG_TRANSFER_LOGFILE, O_WRONLY | O_APPEND | O_CREAT, 0622);

	if (fd == -1) return;

	set_bin(fd);
	write(fd, data, len < 0 ? strlen(data) : len);
	close(fd);
}
#undef DEBUG_TRANSFER_LOGFILE

#else
#define debug_transfer_log(data, len)
#endif


void dns_found(/* struct connection */ void *, int);
static void connected(/* struct connection */ void *);

void
close_socket(struct connection *conn, struct connection_socket *socket)
{
	if (socket->fd == -1) return;
#ifdef CONFIG_SSL
	if (conn && socket->ssl) ssl_close(conn, socket);
#endif
	close(socket->fd);
	clear_handlers(socket->fd);
	socket->fd = -1;
}

void
dns_exception(void *data)
{
	struct connection *conn = data;

	set_connection_state(conn, S_EXCEPT);
	close_socket(NULL, conn->conn_info->socket);
	dns_found(conn, 0);
}

static void
exception(void *data)
{
	retry_conn_with_state((struct connection *) data, S_EXCEPT);
}


struct conn_info *
init_connection_info(struct uri *uri, struct connection_socket *socket,
		     void (*done)(struct connection *))
{
	struct conn_info *conn_info = mem_calloc(1, sizeof(*conn_info));

	if (!conn_info) return NULL;

	conn_info->done = done;
	conn_info->socket = socket;
	conn_info->port = get_uri_port(uri);
	conn_info->triedno = -1;
	conn_info->addr = NULL;

	return conn_info;
}

void
done_connection_info(struct connection *conn)
{
	struct conn_info *conn_info = conn->conn_info;

	conn->conn_info = NULL;

	if (conn_info->done) conn_info->done(conn);

	mem_free_if(conn_info->addr);
	mem_free(conn_info);
}

void
make_connection(struct connection *conn, struct connection_socket *socket,
		void (*done)(struct connection *))
{
	unsigned char *host = get_uri_string(conn->uri, URI_DNS_HOST);
	struct conn_info *conn_info;
	int async;

	if (!host) {
		retry_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}

	conn_info = init_connection_info(conn->uri, socket, done);
	if (!conn_info) {
		mem_free(host);
		retry_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}

	conn->conn_info = conn_info;

	debug_transfer_log("\nCONNECTION: ", -1);
	debug_transfer_log(host, -1);
	debug_transfer_log("\n", -1);

	if (conn->cache_mode >= CACHE_MODE_FORCE_RELOAD)
		async = find_host_no_cache(host, &conn_info->addr, &conn_info->addrno,
					   &conn->dnsquery, dns_found, conn);
	else
		async = find_host(host, &conn_info->addr, &conn_info->addrno,
				  &conn->dnsquery, dns_found, conn);

	mem_free(host);

	if (async) set_connection_state(conn, S_DNS);
}


/* Returns negative if error, otherwise pasv socket's fd. */
int
get_pasv_socket(struct connection *conn, int ctrl_sock, unsigned char *port)
{
	struct sockaddr_in sa, sb;
	int sock, len;

	memset(&sa, 0, sizeof(sa));
	memset(&sb, 0, sizeof(sb));

	/* Get our endpoint of the control socket */
	len = sizeof(sa);
	if (getsockname(ctrl_sock, (struct sockaddr *) &sa, &len)) {
sock_error:
		retry_conn_with_state(conn, -errno);
		return -1;
	}

	/* Get a passive socket */

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
		goto sock_error;

	/* Set it non-blocking */

	if (set_nonblocking_fd(sock) < 0)
		goto sock_error;

	/* Bind it to some port */

	copy_struct(&sb, &sa);
	sb.sin_port = 0;
	if (bind(sock, (struct sockaddr *) &sb, sizeof(sb)))
		goto sock_error;

	/* Get our endpoint of the passive socket and save it to port */

	len = sizeof(sa);
	if (getsockname(sock, (struct sockaddr *) &sa, &len))
		goto sock_error;
	memcpy(port, &sa.sin_addr.s_addr, 4);
	memcpy(port + 4, &sa.sin_port, 2);

	/* Go listen */

	if (listen(sock, 1))
		goto sock_error;

	set_ip_tos_throughput(sock);

	return sock;
}

#ifdef CONFIG_IPV6
int
get_pasv6_socket(struct connection *conn, int ctrl_sock,
		 struct sockaddr_storage *s6)
{
	int sock;
	struct sockaddr_in6 s0;
	int len = sizeof(s0);

	memset(&s0, 0, sizeof(s0));
	memset(s6, 0, sizeof(*s6));

	/* Get our endpoint of the control socket */

	if (getsockname(ctrl_sock, (struct sockaddr *) s6, &len)) {
sock_error:
		retry_conn_with_state(conn, -errno);
		return -1;
	}

	/* Get a passive socket */

	sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
		goto sock_error;

	/* Set it non-blocking */

	if (set_nonblocking_fd(sock) < 0)
		goto sock_error;

	/* Bind it to some port */

	memcpy(&s0, s6, sizeof(s0));
	s0.sin6_port = 0;
	if (bind(sock, (struct sockaddr *) &s0, sizeof(s0)))
		goto sock_error;

	/* Get our endpoint of the passive socket and save it to port */

	len = sizeof(s0);
	if (getsockname(sock, (struct sockaddr *) s6, &len))
		goto sock_error;

	/* Go listen */

	if (listen(sock, 1))
		goto sock_error;

	set_ip_tos_throughput(sock);

	return sock;
}

static inline int
check_if_local_address6(struct sockaddr_in6 *addr)
{
	struct ifaddrs *ifaddrs;
	int local = IN6_IS_ADDR_LOOPBACK(&(addr->sin6_addr));

	if (!local && !getifaddrs(&ifaddrs)) {
		struct ifaddrs *ifa;

		for (ifa = ifaddrs; ifa; ifa = ifa->ifa_next) {
			if (!ifa->ifa_addr)
				continue;

			if (ifa->ifa_addr->sa_family == AF_INET6
			    && !memcmp(&addr->sin6_addr.s6_addr,
			    &((struct sockaddr_in6 *) ifa->ifa_addr)->sin6_addr.s6_addr,
			    sizeof(addr->sin6_addr.s6_addr))) {
				local = 1;
				break;
			}

			if (ifa->ifa_addr->sa_family == AF_INET
			    && !memcmp(&((struct sockaddr_in *) &addr)->sin_addr.s_addr,
				&((struct sockaddr_in *) ifa->ifa_addr)->sin_addr.s_addr,
				sizeof(((struct sockaddr_in *) &addr)->sin_addr.s_addr))) {
					local = 1;
					break;
			}
		}

		freeifaddrs(ifaddrs);
	}

	return local;
}
#endif /* CONFIG_IPV6 */

static inline int
check_if_local_address4(struct sockaddr_in *addr)
{
	struct ifaddrs *ifaddrs;
	int local = (ntohl(addr->sin_addr.s_addr) >> 24) == IN_LOOPBACKNET;

	if (!local && !getifaddrs(&ifaddrs)) {
		struct ifaddrs *ifa;

		for (ifa = ifaddrs; ifa; ifa = ifa->ifa_next) {
			if (!ifa->ifa_addr)
				continue;

			if (ifa->ifa_addr->sa_family != AF_INET) continue;

			if (!memcmp(&addr->sin_addr.s_addr,
				&((struct sockaddr_in *) ifa->ifa_addr)->sin_addr.s_addr,
				sizeof(addr->sin_addr.s_addr))) {
					local = 1;
					break;
			}
		}

		freeifaddrs(ifaddrs);
	}

	return local;
}


void
dns_found(void *data, int state)
{
	int sock = -1;
	struct connection *conn = (struct connection *) data;
	struct conn_info *conn_info = conn->conn_info;
	int i;
	int trno = conn_info->triedno;
	int only_local = get_cmd_opt_bool("localhost");
	int saved_errno = 0;
	int at_least_one_remote_ip = 0;
	/* We tried something but we failed in such a way that we would rather
	 * prefer the connection to retain the information about previous
	 * failures.  That is, we i.e. decided we are forbidden to even think
	 * about such a connection attempt.
	 * XXX: Unify with @local_only handling? --pasky */
	int silent_fail = 0;

	if (state < 0) {
		abort_conn_with_state(conn, S_NO_DNS);
		return;
	}

	/* Clear handlers, the connection to the previous RR really timed
	 * out and doesn't interest us anymore. */
	if (conn_info->socket && conn_info->socket->fd >= 0)
		clear_handlers(conn_info->socket->fd);

	for (i = conn_info->triedno + 1; i < conn_info->addrno; i++) {
#ifdef CONFIG_IPV6
		struct sockaddr_in6 addr = *((struct sockaddr_in6 *) &conn_info->addr[i]);
#else
		struct sockaddr_in addr = *((struct sockaddr_in *) &conn_info->addr[i]);
#endif
		int family;
		int force_family = conn->uri->ip_family;

#ifdef CONFIG_IPV6
		family = addr.sin6_family;
#else
		family = addr.sin_family;
#endif

		conn_info->triedno++;

		if (only_local) {
			int local = 0;
#ifdef CONFIG_IPV6
			if (addr.sin6_family == AF_INET6)
				local = check_if_local_address6((struct sockaddr_in6 *) &addr);
			else
#endif
				local = check_if_local_address4((struct sockaddr_in *) &addr);

			/* This forbids connections to anything but local, if option is set. */
			if (!local) {
				at_least_one_remote_ip = 1;
				continue;
			}
		}

#ifdef CONFIG_IPV6
		if (family == AF_INET6 && (!get_opt_bool("connection.try_ipv6") || (force_family && force_family != 6))) {
			silent_fail = 1;
			continue;
		} else
#endif
		if (family == AF_INET && (!get_opt_bool("connection.try_ipv4") || (force_family && force_family != 4))) {
			silent_fail = 1;
			continue;
		}
		silent_fail = 0;

		sock = socket(family, SOCK_STREAM, IPPROTO_TCP);
		if (sock == -1) {
			if (errno && !saved_errno) saved_errno = errno;
			continue;
		}

		if (set_nonblocking_fd(sock) < 0) {
			if (errno && !saved_errno) saved_errno = errno;
			close(sock);
			continue;
		}
		conn_info->socket->fd = sock;

#ifdef CONFIG_IPV6
		addr.sin6_port = htons(conn_info->port);
#else
		addr.sin_port = htons(conn_info->port);
#endif

		/* We can set conn->protocol_family here even if the connection
		 * will fail, as we will use it only when it will be successfully
		 * established. At least I hope that noone else will want to do
		 * something else ;-). --pasky */

#ifdef CONFIG_IPV6
		if (addr.sin6_family == AF_INET6) {
			conn->protocol_family = 1;
			if (connect(sock, (struct sockaddr *) &addr,
					sizeof(struct sockaddr_in6)) == 0)
				break;
		} else
#endif
		{
			conn->protocol_family = 0;
			if (connect(sock, (struct sockaddr *) &addr,
					sizeof(struct sockaddr_in)) == 0)
				break; /* success */
		}

		if (errno == EALREADY
#ifdef EWOULDBLOCK
		    || errno == EWOULDBLOCK
#endif
		    || errno == EINPROGRESS) {
			/* It will take some more time... */
			set_handlers(sock, NULL, connected, dns_exception, conn);
			set_connection_state(conn, S_CONN);
			return;
		}

		if (errno && !saved_errno) saved_errno = errno;

		close(sock);
	}

	if (i >= conn_info->addrno) {
		/* Tried everything, but it didn't help :(. */

		if (only_local && !saved_errno && at_least_one_remote_ip) {
			/* Yes we might hit a local address and fail in the
			 * process, but what matters is the last one because
			 * we do not know the previous one's errno, and the
			 * added complexity wouldn't really be worth it. */
			abort_conn_with_state(conn, S_LOCAL_ONLY);
			return;
		}

		/* We set new state only if we already tried something new. */
		if (trno != conn_info->triedno && !silent_fail)
			set_connection_state(conn, -errno);
		else if (trno == -1 && silent_fail)
 			/* All failed. */
			set_connection_state(conn, S_NO_FORCED_DNS);

		retry_connection(conn);
		return;
	}

#ifdef CONFIG_SSL
	/* Check if the connection should run over an encrypted link */
	if (get_protocol_need_ssl(conn->uri->protocol)
	    && ssl_connect(conn, conn_info->socket) < 0)
		return;
#endif

	done_connection_info(conn);
}

static void
connected(void *data)
{
	struct connection *conn = (struct connection *) data;
	struct conn_info *conn_info = conn->conn_info;
	struct connection_socket *socket = conn_info->socket;
	int err = 0;
	int len = sizeof(err);

	assertm(conn_info, "Lost conn_info!");
	if_assert_failed return;

	if (getsockopt(socket->fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len) == 0) {
		/* Why does EMX return so large values? */
		if (err >= 10000) err -= 10000;
	} else {
		/* getsockopt() failed */
		if (errno > 0)
			err = errno;
		else
			err = -(S_STATE);
	}

	if (err > 0) {
		set_connection_state(conn, -err);

		/* There are maybe still some more candidates. */
		close_socket(NULL, socket);
		dns_found(conn, 0);
		return;
	}

#ifdef CONFIG_SSL
	/* Check if the connection should run over an encrypted link */
	if (get_protocol_need_ssl(conn->uri->protocol)
	    && ssl_connect(conn, socket) < 0)
		return;
#endif

	done_connection_info(conn);
}


struct write_buffer {
	/* A routine called when all the data is sent (therefore this is
	 * _different_ from read_buffer.done !). */
	void (*done)(struct connection *);

	struct connection_socket *socket;
	int len;
	int pos;

	unsigned char data[1]; /* must be at end of struct */
};

static void
write_select(struct connection *conn)
{
	struct write_buffer *wb = conn->buffer;
	int wr;

	assertm(wb, "write socket has no buffer");
	if_assert_failed {
		abort_conn_with_state(conn, S_INTERNAL);
		return;
	}

	/* We are making some progress, therefore reset the timeout; ie.  when
	 * uploading large files the time needed for all the data to be sent
	 * can easily exceed the timeout. We don't need to do this for
	 * read_select() because it calls user handler every time new data is
	 * acquired and the user handler does this. */
	set_connection_timeout(conn);

#if 0
	printf("ws: %d\n",wb->len-wb->pos);
	for (wr = wb->pos; wr < wb->len; wr++) printf("%c", wb->data[wr]);
	printf("-\n");
#endif

#ifdef CONFIG_SSL
	if (wb->socket->ssl) {
		wr = ssl_write(conn, wb->socket, wb->data + wb->pos, wb->len - wb->pos);
		if (wr <= 0) return;
	} else
#endif
	{
		assert(wb->len - wb->pos > 0);
		wr = safe_write(wb->socket->fd, wb->data + wb->pos, wb->len - wb->pos);
		if (wr <= 0) {
			retry_conn_with_state(conn, wr ? -errno : S_CANT_WRITE);
			return;
		}
	}

	/*printf("wr: %d\n", wr);*/
	wb->pos += wr;
	if (wb->pos == wb->len) {
		void (*f)(struct connection *) = wb->done;

		conn->buffer = NULL;
		clear_handlers(wb->socket->fd);
		mem_free(wb);
		f(conn);
	}
}

void
write_to_socket(struct connection *conn, struct connection_socket *socket,
		unsigned char *data, int len, void (*done)(struct connection *))
{
	struct write_buffer *wb;

	debug_transfer_log(data, len);

	assert(len > 0);
	if_assert_failed return;

	wb = mem_alloc(sizeof(*wb) + len);
	if (!wb) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}

	wb->socket = socket;
	wb->len = len;
	wb->pos = 0;
	wb->done = done;
	memcpy(wb->data, data, len);
	mem_free_set(&conn->buffer, wb);
	set_handlers(socket->fd, NULL, (void *) write_select, (void *) exception, conn);
}

#define RD_ALLOC_GR (2<<11) /* 4096 */
#define RD_MEM(rb) (sizeof(*(rb)) + 4 * RD_ALLOC_GR + RD_ALLOC_GR)
#define RD_SIZE(rb, len) ((RD_MEM(rb) + (len)) & ~(RD_ALLOC_GR - 1))

static void
read_select(struct connection *conn)
{
	struct read_buffer *rb = conn->buffer;
	int rd;

	assertm(rb, "read socket has no buffer");
	if_assert_failed {
		abort_conn_with_state(conn, S_INTERNAL);
		return;
	}

	/* XXX: Should we set_connection_timeout() as we do in write_select()?
	 * --pasky */

	clear_handlers(rb->socket->fd);

	if (!rb->freespace) {
		int size = RD_SIZE(rb, rb->len);

		rb = mem_realloc(rb, size);
		if (!rb) {
			abort_conn_with_state(conn, S_OUT_OF_MEM);
			return;
		}
		rb->freespace = size - sizeof(*rb) - rb->len;
		assert(rb->freespace > 0);
		conn->buffer = rb;
	}

#ifdef CONFIG_SSL
	if (rb->socket->ssl) {
		rd = ssl_read(conn, rb->socket, rb);
		if (rd <= 0) return;
	} else
#endif
	{
		rd = safe_read(rb->socket->fd, rb->data + rb->len, rb->freespace);
		if (rd <= 0) {
			if (rb->close != READ_BUFFER_RETRY_ONCLOSE && !rd) {
				rb->close = READ_BUFFER_END;
				rb->done(conn, rb);
				return;
			}

			retry_conn_with_state(conn, rd ? -errno : S_CANT_READ);
			return;
		}
	}

	debug_transfer_log(rb->data + rb->len, rd);

	rb->len += rd;
	rb->freespace -= rd;
	assert(rb->freespace >= 0);

	rb->done(conn, rb);
}

struct read_buffer *
alloc_read_buffer(struct connection *conn)
{
	struct read_buffer *rb;

	rb = mem_calloc(1, RD_SIZE(rb, 0));
	if (!rb) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return NULL;
	}

	rb->freespace = RD_SIZE(rb, 0) - sizeof(*rb);

	return rb;
}

#undef RD_ALLOC_GR
#undef RD_MEM
#undef RD_SIZE

void
read_from_socket(struct connection *conn, struct connection_socket *socket,
		 struct read_buffer *buffer,
		 void (*done)(struct connection *, struct read_buffer *))
{
	buffer->done = done;
	buffer->socket = socket;

	if (conn->buffer && buffer != conn->buffer)
		mem_free(conn->buffer);
	conn->buffer = buffer;

	set_handlers(socket->fd, (void *) read_select, NULL, (void *) exception, conn);
}

void
kill_buffer_data(struct read_buffer *rb, int n)
{
	assertm(n >= 0 && n <= rb->len, "bad number of bytes: %d", n);
	if_assert_failed { rb->len = 0;  return; }

	if (!n) return; /* FIXME: We accept to kill 0 bytes... */
	rb->len -= n;
	memmove(rb->data, rb->data + n, rb->len);
	rb->freespace += n;
}
