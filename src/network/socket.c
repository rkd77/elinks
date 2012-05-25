/* Sockets-o-matic */

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
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h> /* socklen_t for MinGW */
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

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "elinks.h"

#include "config/options.h"
#include "main/select.h"
#include "network/connection.h"
#include "network/dns.h"
#include "network/socket.h"
#include "network/ssl/socket.h"
#include "osdep/osdep.h"
#include "osdep/getifaddrs.h"
#include "protocol/http/blacklist.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


/* Holds information used during the connection establishing phase. */
struct connect_info {
	struct sockaddr_storage *addr;	 /* Array of found addresses. */
	int addrno;			 /* Number of found addresses. */
	int triedno;			 /* Index of last tried address */
	socket_connect_T done;		 /* Callback signaled when connected. */
	void *dnsquery;			 /* Pointer to DNS query info. */
	int port;			 /* Which port to bind to. */
	int ip_family;			 /* If non-zero, force to IP version. */
	struct uri *uri;		 /* For updating the blacklist. */
};

/** For detecting whether a struct socket has been deleted while a
 * function was using it.  */
struct socket_weak_ref {
	LIST_HEAD(struct socket_weak_ref);

	/** done_socket() resets this to NULL.  */
	struct socket *socket;
};

static INIT_LIST_OF(struct socket_weak_ref, socket_weak_refs);

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


static struct connect_info *
init_connection_info(struct uri *uri, struct socket *socket,
		     socket_connect_T connect_done)
{
	struct connect_info *connect_info = mem_calloc(1, sizeof(*connect_info));

	if (!connect_info) return NULL;

	connect_info->done = connect_done;
	connect_info->port = get_uri_port(uri);
	connect_info->ip_family = uri->ip_family;
	connect_info->triedno = -1;
	connect_info->addr = NULL;
	connect_info->uri = get_uri_reference(uri);

	return connect_info;
}

static void
done_connection_info(struct socket *socket)
{
	struct connect_info *connect_info = socket->connect_info;

	assert(socket->connect_info);

	if (connect_info->dnsquery) kill_dns_request(&connect_info->dnsquery);

	mem_free_if(connect_info->addr);
	done_uri(connect_info->uri);
	mem_free_set(&socket->connect_info, NULL);
}

struct socket *
init_socket(void *conn, struct socket_operations *ops)
{
	struct socket *socket;

	socket = mem_calloc(1, sizeof(*socket));
	if (!socket) return NULL;

	socket->fd = -1;
	socket->conn = conn;
	socket->ops = ops;

	return socket;
}

void
done_socket(struct socket *socket)
{
	struct socket_weak_ref *ref;

	close_socket(socket);

	if (socket->connect_info)
		done_connection_info(socket);

	mem_free_set(&socket->read_buffer, NULL);
	mem_free_set(&socket->write_buffer, NULL);

	foreach(ref, socket_weak_refs) {
		if (ref->socket == socket)
			ref->socket = NULL;
	}
}

void
close_socket(struct socket *socket)
{
	if (socket->fd == -1) return;
#ifdef CONFIG_SSL
	if (socket->ssl) ssl_close(socket);
#endif
	close(socket->fd);
	clear_handlers(socket->fd);
	socket->fd = -1;
}

void
dns_exception(struct socket *socket)
{
	connect_socket(socket, connection_state(S_EXCEPT));
}

static void
exception(struct socket *socket)
{
	socket->ops->retry(socket, connection_state(S_EXCEPT));
}


void
timeout_socket(struct socket *socket)
{
	if (!socket->connect_info) {
		socket->ops->retry(socket, connection_state(S_TIMEOUT));
		return;
	}

	/* Is the DNS resolving still in progress? */
	if (socket->connect_info->dnsquery) {
		socket->ops->done(socket, connection_state(S_TIMEOUT));
		return;
	}

	/* Try the next address, */
	connect_socket(socket, connection_state(S_TIMEOUT));

	/* Reset the timeout if connect_socket() started a new attempt
	 * to connect. */
	if (socket->connect_info)
		socket->ops->set_timeout(socket, connection_state(0));
}


/* DNS callback. */
static void
dns_found(struct socket *socket, struct sockaddr_storage *addr, int addrlen)
{
	struct connect_info *connect_info = socket->connect_info;
	int size;

	if (!addr) {
		socket->ops->done(socket, connection_state(S_NO_DNS));
		return;
	}

	assert(connect_info);

	size = sizeof(*addr) * addrlen;

	connect_info->addr = mem_alloc(size);
	if (!connect_info->addr) {
		socket->ops->done(socket, connection_state(S_OUT_OF_MEM));
		return;
	}

	memcpy(connect_info->addr, addr, size);
	connect_info->addrno = addrlen;

	/* XXX: Passing non-result state here is bad but a lack of alternatives
	 * makes it so. Well adding get_state() socket operation could maybe fix
	 * it but the returned state would most likely be a non-result one at
	 * this point in the connection lifecycle. This will, however, only be a
	 * problem if connect_socket() fails without doing any system calls
	 * which is only the case when forcing the IP family. So it is better to
	 * handle it in connect_socket(). */
	connect_socket(socket, connection_state(S_CONN));
}

void
make_connection(struct socket *socket, struct uri *uri,
		socket_connect_T connect_done, int no_cache)
{
	unsigned char *host = get_uri_string(uri, URI_DNS_HOST);
	struct connect_info *connect_info;
	enum dns_result result;

	socket->ops->set_timeout(socket, connection_state(0));

	if (!host) {
		socket->ops->retry(socket, connection_state(S_OUT_OF_MEM));
		return;
	}

	connect_info = init_connection_info(uri, socket, connect_done);
	if (!connect_info) {
		mem_free(host);
		socket->ops->retry(socket, connection_state(S_OUT_OF_MEM));
		return;
	}

	socket->connect_info = connect_info;
	/* XXX: Keep here and not in init_connection_info() to make
	 * complete_connect_socket() work from the HTTP implementation. */
	socket->need_ssl = get_protocol_need_ssl(uri->protocol);
	if (!socket->set_no_tls) {
		enum blacklist_flags flags = get_blacklist_flags(uri);
		socket->no_tls = ((flags & SERVER_BLACKLIST_NO_TLS) != 0);
		socket->set_no_tls = 1;
	}

	debug_transfer_log("\nCONNECTION: ", -1);
	debug_transfer_log(host, -1);
	debug_transfer_log("\n", -1);

	result = find_host(host, &connect_info->dnsquery, (dns_callback_T) dns_found,
			   socket, no_cache);

	mem_free(host);

	if (result == DNS_ASYNC)
		socket->ops->set_state(socket, connection_state(S_DNS));
}


/* Returns negative if error, otherwise pasv socket's fd. */
int
get_pasv_socket(struct socket *ctrl_socket, struct sockaddr_storage *addr)
{
	struct sockaddr_in bind_addr4;
	struct sockaddr *bind_addr;
	struct sockaddr *pasv_addr = (struct sockaddr *) addr;
	size_t addrlen;
	int sock = -1;
	int syspf; /* Protocol Family given to system, not EL_PF_... */
	socklen_t len;
#ifdef CONFIG_IPV6
	struct sockaddr_in6 bind_addr6;

	if (ctrl_socket->protocol_family == EL_PF_INET6) {
		bind_addr = (struct sockaddr *) &bind_addr6;
		addrlen   = sizeof(bind_addr6);
		syspf     = PF_INET6;
	} else
#endif
	{
		bind_addr = (struct sockaddr *) &bind_addr4;
		addrlen   = sizeof(bind_addr4);
		syspf     = PF_INET;
	}

	memset(pasv_addr, 0, addrlen);
	memset(bind_addr, 0, addrlen);

	/* Get our endpoint of the control socket */
	len = addrlen;
	if (getsockname(ctrl_socket->fd, pasv_addr, &len)) {
sock_error:
		if (sock != -1) close(sock);
		ctrl_socket->ops->retry(ctrl_socket,
					connection_state_for_errno(errno));
		return -1;
	}

	/* Get a passive socket */

	sock = socket(syspf, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
		goto sock_error;

	/* Set it non-blocking */

	if (set_nonblocking_fd(sock) < 0)
		goto sock_error;

	/* Bind it to some port */

	memcpy(bind_addr, pasv_addr, addrlen);
#ifdef CONFIG_IPV6
	if (ctrl_socket->protocol_family == EL_PF_INET6)
		bind_addr6.sin6_port = 0;
	else
#endif
		bind_addr4.sin_port = 0;

	if (bind(sock, bind_addr, addrlen))
		goto sock_error;

	/* Get our endpoint of the passive socket and save it to port */

	len = addrlen;
	if (getsockname(sock, pasv_addr, &len))
		goto sock_error;

	/* Go listen */

	if (listen(sock, 1))
		goto sock_error;

	set_ip_tos_throughput(sock);

	return sock;
}

#ifdef CONFIG_IPV6
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
complete_connect_socket(struct socket *socket, struct uri *uri,
			socket_connect_T done)
{
	struct connect_info *connect_info = socket->connect_info;

	if (connect_info && connect_info->uri) {
		/* Remember whether the server supported TLS or not.
		 * Then the next request can immediately use the right
		 * protocol.  This is important for HTTP POST requests
		 * because it is not safe to silently retry them.  The
		 * uri parameter is normally NULL here so don't use it.  */
		if (socket->no_tls)
			add_blacklist_entry(connect_info->uri,
					    SERVER_BLACKLIST_NO_TLS);
		else
			del_blacklist_entry(connect_info->uri,
					    SERVER_BLACKLIST_NO_TLS);
	}

	/* This is a special case used by the HTTP implementation to acquire an
	 * SSL link for handling CONNECT requests. */
	if (!connect_info) {
		assert(uri && socket);
		connect_info = init_connection_info(uri, socket, done);
		if (!connect_info) {
			socket->ops->done(socket, connection_state(S_OUT_OF_MEM));
			return;
		}

		socket->connect_info = connect_info;
	}

#ifdef CONFIG_SSL
	/* Check if the connection should run over an encrypted link */
	if (socket->need_ssl
	    && !socket->ssl
	    && ssl_connect(socket) < 0)
		return;
#endif

	if (connect_info->done)
		connect_info->done(socket);

	done_connection_info(socket);
}

/* Select handler which is set for the socket descriptor when connect() has
 * indicated (via errno) that it is in progress. On completion this handler gets
 * called. */
static void
connected(struct socket *socket)
{
	int err = 0;
	struct connection_state state = connection_state(0);
	socklen_t len = sizeof(err);

	assertm(socket->connect_info != NULL, "Lost connect_info!");
	if_assert_failed return;

	if (getsockopt(socket->fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len) == 0) {
		/* Why does EMX return so large values? */
		if (err >= 10000) err -= 10000;
		if (err != 0)
			state = connection_state_for_errno(err);
		else
			state = connection_state(0);
	} else {
		/* getsockopt() failed */
		if (errno != 0)
			state = connection_state_for_errno(errno);
		else
			state = connection_state(S_STATE);
	}

	if (!is_in_state(state, 0)) {
		/* There are maybe still some more candidates. */
		connect_socket(socket, state);
		return;
	}

	complete_connect_socket(socket, NULL, NULL);
}

void
connect_socket(struct socket *csocket, struct connection_state state)
{
	int sock = -1;
	struct connect_info *connect_info = csocket->connect_info;
	int i;
	int trno = connect_info->triedno;
	int only_local = get_cmd_opt_bool("localhost");
	int saved_errno = 0;
	int at_least_one_remote_ip = 0;
#ifdef CONFIG_IPV6
	int try_ipv6 = get_opt_bool("connection.try_ipv6", NULL);
#endif
	int try_ipv4 = get_opt_bool("connection.try_ipv4", NULL);
	/* We tried something but we failed in such a way that we would rather
	 * prefer the connection to retain the information about previous
	 * failures.  That is, we i.e. decided we are forbidden to even think
	 * about such a connection attempt.
	 * XXX: Unify with @local_only handling? --pasky */
	int silent_fail = 0;

	csocket->ops->set_state(csocket, state);

	/* Clear handlers, the connection to the previous RR really timed
	 * out and doesn't interest us anymore. */
	if (csocket->fd >= 0)
		close_socket(csocket);

	for (i = connect_info->triedno + 1; i < connect_info->addrno; i++) {
#ifdef CONFIG_IPV6
		struct sockaddr_in6 addr = *((struct sockaddr_in6 *) &connect_info->addr[i]);
		int family = addr.sin6_family;
#else
		struct sockaddr_in addr = *((struct sockaddr_in *) &connect_info->addr[i]);
		int family = addr.sin_family;
#endif
		int pf;
		int force_family = connect_info->ip_family;

		connect_info->triedno++;

		if (only_local) {
			int local = 0;
#ifdef CONFIG_IPV6
			if (family == AF_INET6)
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
		if (family == AF_INET6) {
			if (!try_ipv6 || (force_family && force_family != 6)) {
				silent_fail = 1;
				continue;
			}
			pf = PF_INET6;

		} else
#endif
		if (family == AF_INET) {
			if (!try_ipv4 || (force_family && force_family != 4)) {
				silent_fail = 1;
				continue;
			}
			pf = PF_INET;

		} else {
			continue;
		}
		silent_fail = 0;

		sock = socket(pf, SOCK_STREAM, IPPROTO_TCP);
		if (sock == -1) {
			if (errno && !saved_errno) saved_errno = errno;
			continue;
		}

		if (set_nonblocking_fd(sock) < 0) {
			if (errno && !saved_errno) saved_errno = errno;
			close(sock);
			continue;
		}
		csocket->fd = sock;

#ifdef CONFIG_IPV6
		addr.sin6_port = htons(connect_info->port);
#else
		addr.sin_port = htons(connect_info->port);
#endif

		/* We can set csocket->protocol_family here even if the connection
		 * will fail, as we will use it only when it will be successfully
		 * established. At least I hope that noone else will want to do
		 * something else ;-). --pasky */
		/* And in fact we must set it early, because of EINPROGRESS.  */

#ifdef CONFIG_IPV6
		if (family == AF_INET6) {
			csocket->protocol_family = EL_PF_INET6;
			if (connect(sock, (struct sockaddr *) &addr,
					sizeof(struct sockaddr_in6)) == 0) {
				/* Success */
				complete_connect_socket(csocket, NULL, NULL);
				return;
			}
		} else
#endif
		{
			csocket->protocol_family = EL_PF_INET;
			if (connect(sock, (struct sockaddr *) &addr,
					sizeof(struct sockaddr_in)) == 0) {
				/* Success */
				complete_connect_socket(csocket, NULL, NULL);
				return;
			}
		}

		if (errno == EALREADY
#ifdef EWOULDBLOCK
		    || errno == EWOULDBLOCK
#endif
		    || errno == EINPROGRESS) {
			/* It will take some more time... */
			set_handlers(sock, NULL, (select_handler_T) connected,
				     (select_handler_T) dns_exception, csocket);
			csocket->ops->set_state(csocket, connection_state(S_CONN));
			return;
		}

		if (errno && !saved_errno) saved_errno = errno;

		close(sock);
	}

	assert(i >= connect_info->addrno);

	/* Tried everything, but it didn't help :(. */

	if (only_local && !saved_errno && at_least_one_remote_ip) {
		/* Yes we might hit a local address and fail in the process, but
		 * what matters is the last one because we do not know the
		 * previous one's errno, and the added complexity wouldn't
		 * really be worth it. */
		csocket->ops->done(csocket, connection_state(S_LOCAL_ONLY));
		return;
	}

	/* Retry reporting the errno state only if we already tried something
	 * new. Else use the S_DNS _progress_ state to make sure that no
	 * download callbacks will report any errors. */
	if (trno != connect_info->triedno && !silent_fail)
		state = connection_state_for_errno(errno);
	else if (trno == -1 && silent_fail)
		/* All failed. */
		state = connection_state(S_NO_FORCED_DNS);

	csocket->ops->retry(csocket, state);
}


struct write_buffer {
	/* A routine called when all the data is sent (therefore this is
	 * _different_ from read_buffer.done !). */
	socket_write_T done;

	int length;
	int pos;

	unsigned char data[1]; /* must be at end of struct */
};

static int
generic_write(struct socket *socket, unsigned char *data, int len)
{
	int wr = safe_write(socket->fd, data, len);

	if (!wr) return SOCKET_CANT_WRITE;

	if (wr < 0) {
#ifdef EWOULDBLOCK
		if (errno == EWOULDBLOCK) return SOCKET_CANT_WRITE;
#endif
		return SOCKET_SYSCALL_ERROR;
	}
	return wr;
}

static void
write_select(struct socket *socket)
{
	struct write_buffer *wb = socket->write_buffer;
	int wr;

	assertm(wb != NULL, "write socket has no buffer");
	if_assert_failed {
		socket->ops->done(socket, connection_state(S_INTERNAL));
		return;
	}

	/* We are making some progress, therefore reset the timeout; ie.  when
	 * uploading large files the time needed for all the data to be sent can
	 * easily exceed the timeout. */
	socket->ops->set_timeout(socket, connection_state(0));

#if 0
	printf("ws: %d\n",wb->length-wb->pos);
	for (wr = wb->pos; wr < wb->length; wr++) printf("%c", wb->data[wr]);
	printf("-\n");
#endif

#ifdef CONFIG_SSL
	if (socket->ssl) {
		wr = ssl_write(socket, wb->data + wb->pos, wb->length - wb->pos);
	} else
#endif
	{
		assert(wb->length - wb->pos > 0);
		wr = generic_write(socket, wb->data + wb->pos, wb->length - wb->pos);
	}

	switch (wr) {
	case SOCKET_CANT_WRITE:
		socket->ops->retry(socket, connection_state(S_CANT_WRITE));
		break;

	case SOCKET_SYSCALL_ERROR:
		socket->ops->retry(socket, connection_state_for_errno(errno));
		break;

	case SOCKET_INTERNAL_ERROR:
		/* The global errno variable is used for passing
		 * internal connection_state error value. */
		socket->ops->done(socket, connection_state(errno));
		break;

	default:
		if (wr < 0) break;

		/*printf("wr: %d\n", wr);*/
		wb->pos += wr;

		if (wb->pos == wb->length) {
			socket_write_T done = wb->done;

			if (!socket->duplex) {
				clear_handlers(socket->fd);

			} else {
				select_handler_T read_handler;
				select_handler_T error_handler;

				read_handler  = get_handler(socket->fd, SELECT_HANDLER_READ);
				error_handler = read_handler
					      ? (select_handler_T) exception
					      : NULL;

				set_handlers(socket->fd, read_handler, NULL,
					     error_handler, socket);
			}

			mem_free_set(&socket->write_buffer, NULL);
			done(socket);
		}
	}
}

void
write_to_socket(struct socket *socket, unsigned char *data, int len,
		struct connection_state state, socket_write_T write_done)
{
	select_handler_T read_handler;
	struct write_buffer *wb;

	debug_transfer_log(data, len);

	assert(len > 0);
	if_assert_failed return;

	socket->ops->set_timeout(socket, connection_state(0));

	wb = mem_alloc(sizeof(*wb) + len);
	if (!wb) {
		socket->ops->done(socket, connection_state(S_OUT_OF_MEM));
		return;
	}

	wb->length = len;
	wb->pos = 0;
	wb->done = write_done;
	memcpy(wb->data, data, len);
	mem_free_set(&socket->write_buffer, wb);

	if (socket->duplex) {
		read_handler = get_handler(socket->fd, SELECT_HANDLER_READ);
	} else {
		read_handler = NULL;
	}

	set_handlers(socket->fd, read_handler, (select_handler_T) write_select,
		     (select_handler_T) exception, socket);
	socket->ops->set_state(socket, state);
}

#define RD_ALLOC_GR (2<<11) /* 4096 */
#define RD_MEM(rb) (sizeof(*(rb)) + 4 * RD_ALLOC_GR + RD_ALLOC_GR)
#define RD_SIZE(rb, len) ((RD_MEM(rb) + (len)) & ~(RD_ALLOC_GR - 1))

static ssize_t
generic_read(struct socket *socket, unsigned char *data, int len)
{
	ssize_t rd = safe_read(socket->fd, data, len);

	if (!rd) return SOCKET_CANT_READ;

	if (rd < 0) {
#ifdef EWOULDBLOCK
		if (errno == EWOULDBLOCK) return SOCKET_CANT_READ;
#endif
		return SOCKET_SYSCALL_ERROR;
	}
	return rd;
}

static void
read_select(struct socket *socket)
{
	struct read_buffer *rb = socket->read_buffer;
	ssize_t rd;

	assertm(rb != NULL, "read socket has no buffer");
	if_assert_failed {
		socket->ops->done(socket, connection_state(S_INTERNAL));
		return;
	}

	/* We are making some progress, therefore reset the timeout; we do this
	 * for read_select() to avoid that the periodic calls to user handlers
	 * has to do it. */
	socket->ops->set_timeout(socket, connection_state(0));

	if (!socket->duplex)
		clear_handlers(socket->fd);

	if (!rb->freespace) {
		int size = RD_SIZE(rb, rb->length);

		rb = mem_realloc(rb, size);
		if (!rb) {
			socket->ops->done(socket, connection_state(S_OUT_OF_MEM));
			return;
		}
		rb->freespace = size - sizeof(*rb) - rb->length;
		assert(rb->freespace > 0);
		socket->read_buffer = rb;
	}

#ifdef CONFIG_SSL
	if (socket->ssl) {
		rd = ssl_read(socket, rb->data + rb->length, rb->freespace);
	} else
#endif
	{
		rd = generic_read(socket, rb->data + rb->length, rb->freespace);
	}

	switch (rd) {
#ifdef CONFIG_SSL
	case SOCKET_SSL_WANT_READ:
		read_from_socket(socket, rb, connection_state(S_TRANS), rb->done);
		break;
#endif
	case SOCKET_CANT_READ:
		if (socket->state != SOCKET_RETRY_ONCLOSE) {
			socket->state = SOCKET_CLOSED;
			rb->done(socket, rb);
			break;
		}

		socket->ops->retry(socket, connection_state(S_CANT_READ));
		break;

	case SOCKET_SYSCALL_ERROR:
		socket->ops->retry(socket, connection_state_for_errno(errno));
		break;

	case SOCKET_INTERNAL_ERROR:
		/* The global errno variable is used for passing
		 * internal connection_state error value. */
		socket->ops->done(socket, connection_state(errno));
		break;

	default:
		debug_transfer_log(rb->data + rb->length, rd);

		rb->length += rd;
		rb->freespace -= rd;
		assert(rb->freespace >= 0);

		rb->done(socket, rb);
	}
}

struct read_buffer *
alloc_read_buffer(struct socket *socket)
{
	struct read_buffer *rb;

	rb = mem_calloc(1, RD_SIZE(rb, 0));
	if (!rb) {
		socket->ops->done(socket, connection_state(S_OUT_OF_MEM));
		return NULL;
	}

	rb->freespace = RD_SIZE(rb, 0) - sizeof(*rb);

	return rb;
}

#undef RD_ALLOC_GR
#undef RD_MEM
#undef RD_SIZE

void
read_from_socket(struct socket *socket, struct read_buffer *buffer,
		 struct connection_state state, socket_read_T done)
{
	const int is_buffer_new = (buffer != socket->read_buffer);
	struct socket_weak_ref ref;
	select_handler_T write_handler;

	ref.socket = socket;
	add_to_list(socket_weak_refs, &ref);

	buffer->done = done;

	socket->ops->set_timeout(socket, connection_state(0));
	socket->ops->set_state(socket, state);

	del_from_list(&ref);
	if (ref.socket == NULL) {
		/* socket->ops->set_state deleted the socket. */
		if (is_buffer_new)
			mem_free(buffer);
		return;
	}

	if (socket->read_buffer && buffer != socket->read_buffer)
		mem_free(socket->read_buffer);
	socket->read_buffer = buffer;

	if (socket->duplex) {
		write_handler = get_handler(socket->fd, SELECT_HANDLER_WRITE);
	} else {
		write_handler = NULL;
	}

	set_handlers(socket->fd, (select_handler_T) read_select, write_handler,
		     (select_handler_T) exception, socket);
}

static void
read_response_from_socket(struct socket *socket)
{
	struct read_buffer *rb = alloc_read_buffer(socket);

	if (rb) read_from_socket(socket, rb, connection_state(S_SENT),
				 socket->read_done);
}

void
request_from_socket(struct socket *socket, unsigned char *data, int datalen,
		    struct connection_state state, enum socket_state sock_state,
		    socket_read_T read_done)
{
	socket->read_done = read_done;
	socket->state = sock_state;
	write_to_socket(socket, data, datalen, state,
			read_response_from_socket);
}

void
kill_buffer_data(struct read_buffer *rb, int n)
{
	assertm(n >= 0 && n <= rb->length, "bad number of bytes: %d", n);
	if_assert_failed { rb->length = 0;  return; }

	if (!n) return; /* FIXME: We accept to kill 0 bytes... */
	rb->length -= n;
	memmove(rb->data, rb->data + n, rb->length);
	rb->freespace += n;
}
