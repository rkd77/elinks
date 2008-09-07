/* BitTorrent peer-wire connection management */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "elinks.h"

#include "config/options.h"
#include "main/select.h"
#include "main/timer.h"
#include "network/connection.h"
#include "network/socket.h"
#include "network/state.h"
#include "osdep/osdep.h"
#include "protocol/bittorrent/common.h"
#include "protocol/bittorrent/peerwire.h"
#include "protocol/bittorrent/peerconnect.h"
#include "protocol/bittorrent/piececache.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "util/bitfield.h"
#include "util/memory.h"
#include "util/string.h"
#include "util/time.h"


/* Only one port is opened and shared between all running BitTorrent
 * connections. This holds the descripter of the listening socket. */
static int bittorrent_socket = -1;

/* The active BitTorrent connections sharing the above listening port. */
static INIT_LIST_OF(struct bittorrent_connection, bittorrent_connections);

/* The incoming (and pending anonymous) peer connections which has not yet been
 * assigned to a BitTorrent connection because the info hash has not been read
 * from the handshake.  */
static INIT_LIST_OF(struct bittorrent_peer_connection, bittorrent_peer_connections);


/* Loop the bittorrent connection list and return matching connection
 * or NULL. */
struct bittorrent_connection *
find_bittorrent_connection(bittorrent_id_T info_hash)
{
	struct bittorrent_connection *bittorrent;

	foreach (bittorrent, bittorrent_connections)
		if (!memcmp(bittorrent->meta.info_hash, info_hash, sizeof(info_hash)))
			return bittorrent;

	return NULL;
}

static void
check_bittorrent_peer_blacklisting(struct bittorrent_peer_connection *peer,
				   struct connection_state state)
{
	enum bittorrent_blacklist_flags flags = BITTORRENT_BLACKLIST_NONE;

	if (bittorrent_id_is_empty(peer->id)
	    || !get_opt_bool("protocol.http.bugs.allow_blacklist"))
		return;

	if (is_system_error(state)) {
		switch (state.syserr) {
		case ECONNREFUSED:
		case ENETUNREACH:
			flags |= BITTORRENT_BLACKLIST_PEER_POOL;
			break;
		}
	} else {
		switch (state.basic) {
		case S_CANT_WRITE:
		case S_CANT_READ:
			if (!peer->local.handshake
			    || !peer->remote.handshake)
				flags |= BITTORRENT_BLACKLIST_PEER_POOL;
			break;

		default:
			break;
		}
	}

	if (flags != BITTORRENT_BLACKLIST_NONE) {
		add_bittorrent_blacklist_flags(peer->id, flags);
	}
}


/* ************************************************************************** */
/* Timeout scheduling: */
/* ************************************************************************** */

/* Timer callback for @peer->timer.  As explained in @install_timer,
 * this function must erase the expired timer ID from all variables.  */
static void
bittorrent_peer_connection_timeout(struct bittorrent_peer_connection *peer)
{
	/* Unset the timer so it won't get stopped when removing the peer
	 * connection. */
	peer->timer = TIMER_ID_UNDEF;
	/* The expired timer ID has now been erased.  */

	done_bittorrent_peer_connection(peer);
}

/* The timeout mechanism is used for both inactive incoming peer connections
 * and peer connections attached to a BitTorrent (master) connection. */
void
set_bittorrent_peer_connection_timeout(struct bittorrent_peer_connection *peer)
{
	milliseconds_T timeout = sec_to_ms(get_opt_int("protocol.bittorrent.peerwire.timeout"));

	kill_timer(&peer->timer);
	install_timer(&peer->timer, timeout,
		      (void (*)(void *)) bittorrent_peer_connection_timeout,
		      peer);
}


/* ************************************************************************** */
/* Socket callback implementation: */
/* ************************************************************************** */

/* Called when the connection changes state. Usually state starts out being
 * S_DMS (while looking up the host) then moves to S_CONN (while connecting),
 * and should hopefully become S_TRANS (while transfering). Note, state can hold
 * both internally defined connection states as described above and errno
 * values, such as ECONNREFUSED. */
static void
set_bittorrent_socket_state(struct socket *socket, struct connection_state state)
{
	struct bittorrent_peer_connection *peer = socket->conn;

	if (is_in_state(state, S_TRANS) && peer->bittorrent)
		set_connection_state(peer->bittorrent->conn,
				     connection_state(S_TRANS));
}

/* Called when progress is made such as when the select() loop detects and
 * schedules reads and writes. The state variable must be ignored. */
static void
set_bittorrent_socket_timeout(struct socket *socket, struct connection_state state)
{
	assert(is_in_state(state, 0));
	set_bittorrent_peer_connection_timeout(socket->conn);
}

/* Called when a non-fatal  error condition has appeared, i.e. the condition is
 * caused by some internal or local system error or simply a timeout. */
static void
retry_bittorrent_socket(struct socket *socket, struct connection_state state)
{
	struct bittorrent_peer_connection *peer = socket->conn;

	check_bittorrent_peer_blacklisting(peer, state);

	/* FIXME: Maybe we should try to reconnect (or simply add connect info
	 * to the peer info list) , but only if we initiated the connection,
	 * i.e. if peer->local.initiater == 1, since it could be just the
	 * tracker probing us. */

	done_bittorrent_peer_connection(peer);
}

/* Called when a fatal and unrecoverable error condition has appeared, such as a
 * DNS query failed. */
static void
done_bittorrent_socket(struct socket *socket, struct connection_state state)
{
	struct bittorrent_peer_connection *peer = socket->conn;

	check_bittorrent_peer_blacklisting(peer, state);

	done_bittorrent_peer_connection(peer);
}

/* All the above socket handlers are attached to the socket via this table. */
static struct socket_operations bittorrent_socket_operations = {
	set_bittorrent_socket_state,
	set_bittorrent_socket_timeout,
	retry_bittorrent_socket,
	done_bittorrent_socket,
};


/* ************************************************************************** */
/* Peer connection management: */
/* ************************************************************************** */

/* Allocate and initialize a basic peer connection either for incoming or
 * outgoing connection. */
static struct bittorrent_peer_connection *
init_bittorrent_peer_connection(int socket)
{
	struct bittorrent_peer_connection *peer;

	peer = mem_calloc(1, sizeof(*peer));
	if (!peer) return NULL;

	peer->socket = init_socket(peer, &bittorrent_socket_operations);
	if (!peer->socket) {
		mem_free(peer);
		return NULL;
	}

	/* We want simultaneous reads and writes. */
	peer->socket->duplex = 1;
	peer->socket->fd = socket;

	init_list(peer->local.requests);
	init_list(peer->remote.requests);
	init_list(peer->queue);

	/* Peers start out being choked and not being interested. */
	peer->local.choked = 1;
	peer->remote.choked = 1;

	return peer;
}

/* Shutdown an remove a peer connection from what ever context it
 * is currently attached, be it the. */
void
done_bittorrent_peer_connection(struct bittorrent_peer_connection *peer)
{
	del_from_list(peer);

	/* The peer might not have been associated with a BitTorrent connection,
	 * yet. */
	if (peer->bittorrent) {
		add_requests_to_bittorrent_piece_cache(peer, &peer->local);

		if (peer->bitfield) {
			remove_bittorrent_peer_from_piece_cache(peer);
			mem_free(peer->bitfield);
		}
	}

	free_list(peer->remote.requests);
	free_list(peer->queue);

	kill_timer(&peer->timer);

	/* Unregister the socket from the select() loop mechanism. */
	done_socket(peer->socket);
	mem_free(peer->socket);

	mem_free(peer);
}


/* Establish connection to a peer. As a backend, it uses the internal and more
 * generic connection creater which takes care of DNS querying etc. */
enum bittorrent_state
make_bittorrent_peer_connection(struct bittorrent_connection *bittorrent,
				struct bittorrent_peer *peer_info)
{
	enum bittorrent_state result = BITTORRENT_STATE_OUT_OF_MEM;
	struct uri *uri = NULL;
	struct string uri_string = NULL_STRING;
	struct bittorrent_peer_connection *peer;

	peer = init_bittorrent_peer_connection(-1);
	if (!peer) goto out;

	peer->local.initiater = 1;

	add_to_list(bittorrent->peers, peer);
	peer->bittorrent = bittorrent;

	peer->bitfield = init_bitfield(bittorrent->meta.pieces);
	if (!peer->bitfield) goto out;

	memcpy(peer->id, peer_info->id, sizeof(peer->id));

	/* XXX: Very hacky; construct a fake URI from which make_connection()
	 * can extract the IP address and port number. */
	/* FIXME: Rather change the make_connection() interface. This is an ugly
	 * hack. */
	if (!init_string(&uri_string)) goto out;
	if (!add_format_to_string(&uri_string,
#ifdef CONFIG_IPV6
				  strchr(peer_info->ip, ':') ?
				  "bittorrent-peer://[%s]:%u/" :
#endif
				  "bittorrent-peer://%s:%u/",
				  peer_info->ip, (unsigned) peer_info->port))
		goto out;
	uri = get_uri(uri_string.source, 0);
	if (!uri) goto out;

	make_connection(peer->socket, uri, send_bittorrent_peer_handshake, 1);
	result = BITTORRENT_STATE_OK;

out:
	if (uri)
		done_uri(uri);
	done_string(&uri_string);
	if (peer && result != BITTORRENT_STATE_OK)
		done_bittorrent_peer_connection(peer);
	return result;
}


/* ************************************************************************** */
/* Listening socket management: */
/* ************************************************************************** */

/* Number of connections to keep in the listening backlog before dropping new
 * ones. */
#define LISTEN_BACKLOG	\
	get_opt_int("protocol.bittorrent.peerwire.connections")

/* Called when we receive a connection on the listening socket. */
static void
accept_bittorrent_peer_connection(void *____)
{
	struct sockaddr_in addr;
	int peer_sock;
	int addrlen = sizeof(addr);
	struct bittorrent_peer_connection *peer;
	struct read_buffer *buffer;

	peer_sock = accept(bittorrent_socket, (struct sockaddr *) &addr, &addrlen);
	if (peer_sock < 0) return;

	if (set_nonblocking_fd(peer_sock) < 0) {
		close(peer_sock);
		return;
	}

	peer = init_bittorrent_peer_connection(peer_sock);
	if (!peer) {
		close(peer_sock);
		return;
	}

	peer->remote.initiater = 1;

	/* Just return. Failure is handled by alloc_read_buffer(). */
	buffer = alloc_read_buffer(peer->socket);
	if (!buffer) return;

	read_from_socket(peer->socket, buffer, connection_state(S_TRANS),
			 read_bittorrent_peer_handshake);

	add_to_list(bittorrent_peer_connections, peer);
}

/* Based on network/socket.c:get_pasv_socket() but modified to try and bind to a
 * port range instead of any port. */
struct connection_state
init_bittorrent_listening_socket(struct connection *conn)
{
	struct bittorrent_connection *bittorrent = conn->info;
	struct sockaddr_in addr, addr2;
	uint16_t port, max_port;
	int len;

	/* XXX: Always add the connection to the list even if we fail so we can
	 * safely assume it is in done_bittorrent_listening_socket(). */
	add_to_list(bittorrent_connections, bittorrent);

	/* Has the socket already been initialized? */
	if (!list_is_singleton(bittorrent_connections))
		return connection_state(S_OK);

	/* We could have bailed out from an earlier attempt. */
	if (bittorrent_socket != -1)
		close(bittorrent_socket);

	bittorrent_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (bittorrent_socket < 0)
		return connection_state_for_errno(errno);

	/* Set it non-blocking */

	if (set_nonblocking_fd(bittorrent_socket) < 0)
		return connection_state_for_errno(errno);

	/* Bind it to some port */

	port	 = get_opt_int("protocol.bittorrent.ports.min");
	max_port = get_opt_int("protocol.bittorrent.ports.max");

	memset(&addr, 0, sizeof(addr));
	addr.sin_port = htons(port);

	/* Repeatedly try the configured port range. */
	while (bind(bittorrent_socket, (struct sockaddr *) &addr, sizeof(addr))) {
		if (errno != EADDRINUSE)
			return connection_state_for_errno(errno);

		/* If all ports was in use fail with EADDRINUSE. */
		if (++port > max_port)
			return connection_state_for_errno(errno);

		memset(&addr, 0, sizeof(addr));
		addr.sin_port = htons(port);
	}

	/* Get the endpoint info about the new socket and save it */

	memset(&addr2, 0, sizeof(addr2));
	len = sizeof(addr2);
	if (getsockname(bittorrent_socket, (struct sockaddr *) &addr2, &len))
		return connection_state_for_errno(errno);

	bittorrent->port = ntohs(addr2.sin_port);

	/* Go listen */

	if (listen(bittorrent_socket, LISTEN_BACKLOG))
		return connection_state_for_errno(errno);

	set_ip_tos_throughput(bittorrent_socket);
	set_handlers(bittorrent_socket, accept_bittorrent_peer_connection,
		     NULL, NULL, NULL);

	return connection_state(S_OK);
}

void
done_bittorrent_listening_socket(struct connection *conn)
{
	struct bittorrent_connection *connection, *bittorrent = conn->info;

	/* The bittorrent connection might not even have been added if the
	 * request for the metainfo file failed so carefully look it up. */
	foreach (connection, bittorrent_connections)
		if (connection == bittorrent) {
			del_from_list(bittorrent);
			break;
		}

	/* If there are no more connections left remove all pending peer
	 * connections. */
	if (list_empty(bittorrent_connections)) {
		struct bittorrent_peer_connection *peer, *next;

		foreachsafe (peer, next, bittorrent_peer_connections)
			done_bittorrent_peer_connection(peer);
	}

	/* Close the listening socket. */
	if (bittorrent_socket != -1) {
		/* Unregister the socket from the select() loop mechanism. */
		clear_handlers(bittorrent_socket);

		close(bittorrent_socket);
		bittorrent_socket = -1;
	}
}
