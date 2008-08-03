/* BitTorrent peer-wire protocol implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
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
#include "osdep/osdep.h"
#include "protocol/bittorrent/bittorrent.h"
#include "protocol/bittorrent/common.h"
#include "protocol/bittorrent/connection.h"
#include "protocol/bittorrent/peerconnect.h"
#include "protocol/bittorrent/peerwire.h"
#include "protocol/bittorrent/piececache.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "util/bitfield.h"
#include "util/memory.h"
#include "util/string.h"


/* I give to you my sweet surrender. */

enum bittorrent_handshake_state {
	BITTORRENT_PEER_HANDSHAKE_OK,		/* Completely read and valid. */
	BITTORRENT_PEER_HANDSHAKE_ERROR,	/* The handshake was invalid. */
	BITTORRENT_PEER_HANDSHAKE_INFO_HASH,	/* All up to the hash was ok. */
	BITTORRENT_PEER_HANDSHAKE_INCOMPLETE,	/* All up to now was correct. */
};

/* The size of the handshake for version 1.0 is:
 * <1:protocolstrlen> <19:protocolstr> <8:reserved> <20:info-hash> <20:peer-id>
 * In total 68 bytes.*/
#define BITTORRENT_PEER_HANDSHAKE_SIZE	(1 + 19 + 8 + 20 + 20)

/* Storing the version identification part of the handshake as one entity
 * (length prefix and string) makes it much easier to verify and write. */
static const bittorrent_id_T BITTORRENT_ID = "\023BitTorrent protocol";

/* Has the last message written to the peer socket been sent or not? */
#define bittorrent_peer_is_sending(peer) ((peer)->socket->write_buffer)


static enum bittorrent_state
do_send_bittorrent_peer_message(struct bittorrent_peer_connection *peer,
				struct bittorrent_peer_request *message);

static enum bittorrent_handshake_state
do_read_bittorrent_peer_handshake(struct socket *socket, struct read_buffer *buffer);


/* ************************************************************************** */
/* Peer state managing: */
/* ************************************************************************** */

/* Queue requests up to the configured limit. */
static void
queue_bittorrent_peer_connection_requests(struct bittorrent_peer_connection *peer)
{
	int size = get_opt_int("protocol.bittorrent.request_queue_size");
	int queue_size = list_size(&peer->local.requests);

	for ( ; queue_size < size; queue_size++) {
		struct bittorrent_peer_request *request;

		request = find_bittorrent_peer_request(peer);
		if (!request) break;

		/* TODO: Insert rarest first? --jonas */
		add_to_list_end(peer->local.requests, request);
	}

	/* Interest in the peer was lost if no request could be queued. */
	if (list_empty(peer->local.requests))
		set_bittorrent_peer_not_interested(peer);
}

/* Called both from the choke period handler of the main BitTorrent connection
 * and upon completion of message sending this handles allocation of peer
 * requests and sending of anything pending. */
/* XXX: Calling this function can cause the peer struct to disappear. */
void
update_bittorrent_peer_connection_state(struct bittorrent_peer_connection *peer)
{
	struct bittorrent_connection *bittorrent = peer->bittorrent;
	struct bittorrent_piece_cache *cache = bittorrent->cache;
	struct bittorrent_peer_request *request, *next_request;
	enum bittorrent_state state;

	/* Drop connections to other seeders or when a partial download has been
	 * completed. */
	if ((peer->remote.seeder && bittorrent->mode == BITTORRENT_MODE_SEEDER)
	    || (cache->partial && cache->partial_pieces == cache->completed_pieces)) {
		done_bittorrent_peer_connection(peer);
		return;
	}

	if (peer->local.interested && !peer->local.choked)
		queue_bittorrent_peer_connection_requests(peer);

	/* Is there a write in progress? */
	if (bittorrent_peer_is_sending(peer)) {
		assert(get_handler(peer->socket->fd, SELECT_HANDLER_WRITE));
		return;
	}

	/* Send the shorter state messages first ... */

	/* Send any peer state oriented messages. */
	foreachsafe (request, next_request, peer->queue) {
		state = do_send_bittorrent_peer_message(peer, request);
		if (state == BITTORRENT_STATE_OK)
			return;
	}

	/* Send local piece requests which has not already been requested. */
	foreachsafe (request, next_request, peer->local.requests) {
		if (request->requested)
			continue;

		request->id = BITTORRENT_MESSAGE_REQUEST;

		state = do_send_bittorrent_peer_message(peer, request);
		if (state == BITTORRENT_STATE_OK)
			return;
	}

	/* Ship the longer piece data messages. */
	foreachsafe (request, next_request, peer->remote.requests) {
		request->id = BITTORRENT_MESSAGE_PIECE;

		state = do_send_bittorrent_peer_message(peer, request);
		if (state == BITTORRENT_STATE_OK)
			return;
	}
}

static inline double
get_bittorrent_rate(struct bittorrent_peer_stats *stats, time_t now,
		    double rate, uint32_t loaded)
{
	return (rate * (stats->last_time - stats->age) + loaded)
		/ (now - stats->age);
}

void
update_bittorrent_peer_connection_stats(struct bittorrent_peer_connection *peer,
					uint32_t downloaded, uint32_t have_piece,
					uint32_t uploaded)
{
	struct bittorrent_peer_stats *stats = &peer->stats;
	time_t now = time(NULL);

	stats->download_rate = get_bittorrent_rate(stats, now, stats->download_rate, downloaded);
	stats->have_rate     = get_bittorrent_rate(stats, now, stats->have_rate, have_piece);

	stats->downloaded   += downloaded;
	stats->uploaded	    += uploaded;
	stats->last_time     = now;

	/* Push the age along, so it will be no older than the requested number
	 * of seconds. */
	if (stats->age < now - 20)
		stats->age = stats->age ? now - 20 : now - 1;
}


/* ************************************************************************** */
/* Peer message handling: */
/* ************************************************************************** */

/* The layout of the length prefixed peer messages.
 *
 * - All indexes and offsets are encoded as 4-bytes big-endian.
 * - Indexes are zero-based.
 * - Variable length fields depends on the message length.
 * - All messages begin with <4:message-length> <1:message-id>
 *   The only exception is the keep-alive message which has length set to zero
 *   and doesn't carry any message ID or payload.
 *
 * Message without payload:
 * ------------------------
 *
 * - choke
 * - unchoke
 * - interested
 * - not-interested
 *
 * Messages with payload:
 * ----------------------
 *
 * - have:		 <4:piece-index>
 * - bitfield:		 <x:bitfield-data>
 * - request and cancel: <4:piece-index> <4:piece-offset> <4:block-length>
 * - piece:		 <4:piece-index> <4:piece-offset> <x:block-data>
 */


/* ************************************************************************** */
/* Peer message sending: */
/* ************************************************************************** */

/* Meesage write completion callback. */
static void
sent_bittorrent_peer_message(struct socket *socket)
{
	assert(!socket->write_buffer);
	/* Check if there are pending messages or requests. */
	update_bittorrent_peer_connection_state(socket->conn);
}

static inline void
add_bittorrent_peer_integer(struct string *string, uint32_t integer)
{
	uint32_t data = htonl(integer);

	add_bytes_to_string(string, (unsigned char *) &data, sizeof(data));
}

/* Common lowlevel backend for composing a peer message and writing it to the
 * socket. */
static enum bittorrent_state
do_send_bittorrent_peer_message(struct bittorrent_peer_connection *peer,
				struct bittorrent_peer_request *message)
{
	struct bittorrent_connection *bittorrent = peer->bittorrent;
	struct string string;
	unsigned char msgid_str[1] = { (unsigned char) message->id };
	uint32_t msglen = 0;

	assert(!bittorrent_peer_is_sending(peer));
	assert(!get_handler(peer->socket->fd, SELECT_HANDLER_WRITE));

	if (!init_string(&string))
		return BITTORRENT_STATE_OUT_OF_MEM;

	/* Reserve 4 bytes to the message length and add the message ID byte. */
	add_bytes_to_string(&string, (unsigned char *) &msglen, sizeof(msglen));

	/* XXX: Can't use add_char_to_string() here because the message ID
	 * can be zero. */
	if (message->id != BITTORRENT_MESSAGE_KEEP_ALIVE)
		add_bytes_to_string(&string, msgid_str, 1);

	switch (message->id) {
	case BITTORRENT_MESSAGE_HAVE:
	{
		add_bittorrent_peer_integer(&string, message->piece);
		assert(string.length == 9);
		break;
	}
	case BITTORRENT_MESSAGE_BITFIELD:
	{
		struct bitfield *bitfield = bittorrent->cache->bitfield;
		size_t bytes = get_bitfield_byte_size(bitfield->bitsize);

		/* Are bitfield messages allowed at this point? */
		assert(!peer->local.bitfield);

		add_bytes_to_string(&string, bitfield->bits, bytes);

		assert(string.length == 5 + bytes);
		break;
	}
	case BITTORRENT_MESSAGE_REQUEST:
	case BITTORRENT_MESSAGE_CANCEL:
	{
		assert(!peer->local.choked);

		add_bittorrent_peer_integer(&string, message->piece);
		add_bittorrent_peer_integer(&string, message->offset);
		add_bittorrent_peer_integer(&string, message->length);

		message->requested = 1;

		assert(string.length == 17);
		break;
	}
	case BITTORRENT_MESSAGE_PIECE:
	{
		unsigned char *data;

		assert(!peer->remote.choked);
		assert(test_bitfield_bit(bittorrent->cache->bitfield, message->piece));

		data = get_bittorrent_piece_cache_data(bittorrent, message->piece);
		if (!data) {
			done_string(&string);
			return BITTORRENT_STATE_CACHE_FAILURE;
		}

		data += message->offset;

		add_bittorrent_peer_integer(&string, message->piece);
		add_bittorrent_peer_integer(&string, message->offset);
		add_bytes_to_string(&string, data, message->length);

		update_bittorrent_peer_connection_stats(peer, 0, 0,
							message->length);
		update_bittorrent_connection_stats(peer->bittorrent,
						   0, message->length, 0);
		assert(string.length == 13 + message->length);
		break;
	}
	case BITTORRENT_MESSAGE_KEEP_ALIVE:
		assert(string.length == 4);
		break;

	case BITTORRENT_MESSAGE_CHOKE:
	case BITTORRENT_MESSAGE_UNCHOKE:
	case BITTORRENT_MESSAGE_INTERESTED:
	case BITTORRENT_MESSAGE_NOT_INTERESTED:
		assert(string.length == 5);
		break;

	default:
		INTERNAL("Bad message ID");
	}

	/* Insert the real message length. */
	msglen = string.length - sizeof(uint32_t);
	msglen = htonl(msglen);
	memcpy(string.source, (unsigned char *) &msglen, sizeof(msglen));

	/* Any message will cause bitfield messages to become invalid. */
	peer->local.bitfield = 1;

	if (message->id != BITTORRENT_MESSAGE_REQUEST) {
		del_from_list(message);
		mem_free(message);
	}

	write_to_socket(peer->socket, string.source, string.length,
			connection_state(S_TRANS),
			sent_bittorrent_peer_message);

	done_string(&string);

	return BITTORRENT_STATE_OK;
}

/* Highlevel backend for sending peer messages. It handles queuing of messages.
 * In order to make this function safe to call from any contexts, messages are
 * NEVER directly written to the peer socket, since that could cause the peer
 * connection struct to disappear from under us. */
void
send_bittorrent_peer_message(struct bittorrent_peer_connection *peer,
			     enum bittorrent_message_id message_id, ...)
{
	struct bittorrent_peer_request message_store, *message = &message_store;
	va_list args;

	memset(message, 0, sizeof(*message));
	message->id = message_id;

	va_start(args, message_id);

	switch (message_id) {
	case BITTORRENT_MESSAGE_CANCEL:
		message->piece  = va_arg(args, uint32_t);
		message->offset = va_arg(args, uint32_t);
		message->length = va_arg(args, uint32_t);
		break;

	case BITTORRENT_MESSAGE_HAVE:
		message->piece = va_arg(args, uint32_t);
		break;

	case BITTORRENT_MESSAGE_BITFIELD:
	case BITTORRENT_MESSAGE_CHOKE:
	case BITTORRENT_MESSAGE_INTERESTED:
	case BITTORRENT_MESSAGE_KEEP_ALIVE:
	case BITTORRENT_MESSAGE_NOT_INTERESTED:
	case BITTORRENT_MESSAGE_UNCHOKE:
		break;

	case BITTORRENT_MESSAGE_REQUEST:
	case BITTORRENT_MESSAGE_PIECE:
		/* Piece and piece request messages are generated automaticalle
		 * from the request queue the local and remote peer status. */
	default:
		INTERNAL("Bad message ID");
	}

	va_end(args);

	message = mem_alloc(sizeof(*message));
	if (!message) return;

	memcpy(message, &message_store, sizeof(*message));

	/* Prioritize bitfield cancel messages by putting them in the start of
	 * the queue so that bandwidth is not wasted. This way bitfield messages
	 * will always be sent before anything else and cancel messages will
	 * always arrive before have messages, which our client prefers. */
	if (message->id == BITTORRENT_MESSAGE_BITFIELD
	    || message->id == BITTORRENT_MESSAGE_CANCEL)
		add_to_list(peer->queue, message);
	else
		add_to_list_end(peer->queue, message);
}


/* ************************************************************************** */
/* Peer message receiving: */
/* ************************************************************************** */

static inline uint32_t
get_bittorrent_peer_integer(struct read_buffer *buffer, int offset)
{
	assert(offset + sizeof(uint32_t) <= buffer->length);
	return ntohl(*((uint32_t *) (buffer->data + offset)));
}

static enum bittorrent_message_id
check_bittorrent_peer_message(struct bittorrent_peer_connection *peer,
			      struct read_buffer *buffer, uint32_t *length)
{
	uint32_t message_length;
	enum bittorrent_message_id message_id;

	*length = 0;

	assert(peer->remote.handshake);

	if (buffer->length < sizeof(message_length))
		return BITTORRENT_MESSAGE_INCOMPLETE;

	message_length = get_bittorrent_peer_integer(buffer, 0);

	if (message_length > get_bittorrent_peerwire_max_message_length())
		return BITTORRENT_MESSAGE_ERROR;

	if (buffer->length - sizeof(message_length) < message_length)
		return BITTORRENT_MESSAGE_INCOMPLETE;

	if (message_length == 0)
		return BITTORRENT_MESSAGE_KEEP_ALIVE;

	message_id = buffer->data[sizeof(message_length)];

	*length = message_length;

	return message_id;
}

static enum bittorrent_state
read_bittorrent_peer_message(struct bittorrent_peer_connection *peer,
			     enum bittorrent_message_id message_id,
			     struct read_buffer *buffer, uint32_t message_length,
			     int *write_errno)
{
	struct bittorrent_connection *bittorrent = peer->bittorrent;
	enum bittorrent_state state;
	uint32_t piece, offset, length;
	unsigned char *data;

	assert(message_id != BITTORRENT_MESSAGE_INCOMPLETE);

	*write_errno = 0;

	switch (message_id) {
	case BITTORRENT_MESSAGE_CHOKE:
		/* Return all pending requests to the free list. */
		peer->local.choked = 1;
		add_requests_to_bittorrent_piece_cache(peer, &peer->local);
		{
			struct bittorrent_peer_request *message, *next;

			foreachsafe (message, next, peer->queue) {
				if (message->id == BITTORRENT_MESSAGE_CANCEL) {
					del_from_list(message);
					mem_free(message);
				}
			}
		}
		break;

	case BITTORRENT_MESSAGE_UNCHOKE:
		peer->local.choked = 0;
		break;

	case BITTORRENT_MESSAGE_INTERESTED:
		peer->remote.interested = 1;
		break;

	case BITTORRENT_MESSAGE_NOT_INTERESTED:
		peer->remote.interested = 0;
		break;

	case BITTORRENT_MESSAGE_HAVE:
		if (message_length < sizeof(uint32_t))
			return BITTORRENT_STATE_ERROR;

		piece = get_bittorrent_peer_integer(buffer, 5);
		/* Is piece out of bound? */
		if (piece >= bittorrent->meta.pieces)
			break;

		/* Is the piece already recorded? */
		if (test_bitfield_bit(peer->bitfield, piece))
			break;

		length = get_bittorrent_piece_length(&bittorrent->meta, piece);
		update_bittorrent_peer_connection_stats(peer, 0, length, 0);

		set_bitfield_bit(peer->bitfield, piece);
		peer->remote.seeder = bitfield_is_set(peer->bitfield);

		update_bittorrent_piece_cache(peer, piece);

		if (peer->local.interested
		    || test_bitfield_bit(bittorrent->cache->bitfield, piece))
			break;

		set_bittorrent_peer_interested(peer);
		break;

	case BITTORRENT_MESSAGE_BITFIELD:
		data   = buffer->data + 5;
		length = message_length - 1; /* Message ID byte. */

		if (length > get_bitfield_byte_size(peer->bitfield->bitsize))
			break;

		/* Are bitfield messages allowed at this point? */
		if (peer->remote.bitfield)
			break;

		/* XXX: The function tail will set the bitfield flag ... */
		copy_bitfield(peer->bitfield, data, length);
		peer->remote.seeder = bitfield_is_set(peer->bitfield);

		update_bittorrent_piece_cache_from_bitfield(peer);

		/* Force checking of the interested flag. */
		foreach_bitfield_set (piece, peer->bitfield) {
		    	if (test_bitfield_bit(bittorrent->cache->bitfield, piece))
				continue;

			set_bittorrent_peer_interested(peer);
			break;
		}
		break;

	case BITTORRENT_MESSAGE_REQUEST:
	case BITTORRENT_MESSAGE_CANCEL:
		if (message_length < sizeof(uint32_t) * 3)
			return BITTORRENT_STATE_ERROR;

		piece  = get_bittorrent_peer_integer(buffer, 5);
		offset = get_bittorrent_peer_integer(buffer, 9);
		length = get_bittorrent_peer_integer(buffer, 13);

		/* FIXME: Should requests be allowed to overlap pieces? */
		if (peer->remote.choked
		    || piece >= bittorrent->meta.pieces
		    || offset + length > get_bittorrent_piece_length(&bittorrent->meta, piece)
		    || !test_bitfield_bit(bittorrent->cache->bitfield, piece))
			break;

		if (length > get_bittorrent_peerwire_max_request_length())
			return BITTORRENT_STATE_ERROR;

		if (message_id == BITTORRENT_MESSAGE_REQUEST) {
			add_bittorrent_peer_request(&peer->remote, piece, offset, length);
		} else {
			del_bittorrent_peer_request(&peer->remote, piece, offset, length);
		}
		break;

	case BITTORRENT_MESSAGE_PIECE:
		if (message_length < sizeof(uint32_t) * 2)
			return BITTORRENT_STATE_ERROR;

		piece  = get_bittorrent_peer_integer(buffer, 5);
		offset = get_bittorrent_peer_integer(buffer, 9);
		length = message_length - 9; /* Msg ID byte + 2 ints */
		data   = buffer->data + 13; /* Offset includes msg len */

		if (peer->local.choked
		    || piece >= bittorrent->meta.pieces
		    || offset + length > get_bittorrent_piece_length(&bittorrent->meta, piece)
		    || length == 0)
			break;

		update_bittorrent_peer_connection_stats(peer, length, 0, 0);
		state = add_to_bittorrent_piece_cache(peer, piece, offset, data, length, write_errno);
		if (state != BITTORRENT_STATE_OK)
			return state;
		break;

	case BITTORRENT_MESSAGE_KEEP_ALIVE:
	default:
		/* Keep-alive messages doesn't require any special handling.
		 * Unknown peer messages are simply dropped. */
		break;
	}

	/* Any message will cause bitfield messages to become invalid. */
	peer->remote.bitfield = 1;

	return BITTORRENT_STATE_OK;
}

static void
read_bittorrent_peer_data(struct socket *socket, struct read_buffer *buffer)
{
	struct bittorrent_peer_connection *peer = socket->conn;

	if (!peer->remote.handshake) {
		enum bittorrent_handshake_state state;

		/* We still needs to read the peer ID from the handshake. */
		state = do_read_bittorrent_peer_handshake(socket, buffer);
		if (state != BITTORRENT_PEER_HANDSHAKE_OK)
			return;

		/* If the handshake was ok'ed see if there are any messages to
		 * read. */
		assert(peer->remote.handshake);
	}

	/* All messages atleast contains an integer prefix. */
	while (buffer->length > sizeof(uint32_t)) {
		enum bittorrent_message_id message_id;
		uint32_t length;
		int write_errno = 0;

		message_id = check_bittorrent_peer_message(peer, buffer, &length);
		if (message_id == BITTORRENT_MESSAGE_INCOMPLETE)
			break;

		if (message_id == BITTORRENT_MESSAGE_ERROR) {
			done_bittorrent_peer_connection(peer);
			return;
		}

		switch (read_bittorrent_peer_message(peer, message_id, buffer, length, &write_errno)) {
		case BITTORRENT_STATE_OK:
			break;

		case BITTORRENT_STATE_OUT_OF_MEM:
			abort_connection(peer->bittorrent->conn,
					 connection_state(S_OUT_OF_MEM));
			return;

		case BITTORRENT_STATE_ERROR:
		default:
			if (!write_errno) {
				done_bittorrent_peer_connection(peer);
				return;
			}

			/* Shutdown on fatal errors! */
			abort_connection(peer->bittorrent->conn,
					 connection_state_for_errno(write_errno));
			return;
		}

		/* Remove the processed data from the input buffer. */
		kill_buffer_data(buffer, length + sizeof(length));
		update_bittorrent_connection_stats(peer->bittorrent, 0, 0,
						   length + sizeof(length));
	}

	update_bittorrent_peer_connection_state(peer);
}


/* ************************************************************************** */
/* Peer handshake exchanging: */
/* ************************************************************************** */

/* XXX: Note, handshake messages are also handled above in the function
 * read_bittorrnt_peer_data() if it is incomplete when reading it below.
 * Often, peers will only send up to and including the info hash, so that the
 * peer ID arrives later. */

/* The handshake was sent, so notify the remote peer about completed piece in a
 * bitfield message and start reading any remaining bits of the handshake or any
 * other message.  */
static void
sent_bittorrent_peer_handshake(struct socket *socket)
{
	struct bittorrent_peer_connection *peer = socket->conn;
	struct read_buffer *buffer = peer->socket->read_buffer;

	assert(buffer);

	/* Only send the bitfield message if there is anything interesting to
	 * report. */
	if (peer->bittorrent->cache->completed_pieces) {
		assert(list_empty(peer->queue));
		send_bittorrent_peer_message(peer, BITTORRENT_MESSAGE_BITFIELD);
	}

	read_from_socket(peer->socket, buffer, connection_state(S_TRANS),
			 read_bittorrent_peer_data);
}

/* This function is called when a handhake has been read from an incoming
 * connection and is used as a callback for when a new peer connection has
 * successfully been established. */
void
send_bittorrent_peer_handshake(struct socket *socket)
{
	struct bittorrent_peer_connection *peer = socket->conn;
	struct bittorrent_connection *bittorrent = peer->bittorrent;
	struct bittorrent_meta *meta = &bittorrent->meta;
	unsigned char reserved[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	unsigned char handshake[BITTORRENT_PEER_HANDSHAKE_SIZE];
	int i = 0;

#define add_to_handshake(handshake, i, data) \
	do { \
		memcpy((handshake) + (i), data, sizeof(data)); \
		i += sizeof(data); \
	} while (0)

	add_to_handshake(handshake, i, BITTORRENT_ID);
	add_to_handshake(handshake, i, reserved);
	add_to_handshake(handshake, i, meta->info_hash);
	add_to_handshake(handshake, i, bittorrent->peer_id);

#undef	add_to_handshake

	assert(handshake[0] == 19);

	if (!peer->socket->read_buffer) {
		/* Just return. Failure is handled by alloc_read_buffer(). */
		peer->socket->read_buffer = alloc_read_buffer(peer->socket);
		if (!peer->socket->read_buffer) {
			done_bittorrent_peer_connection(peer);
			return;
		}
	}

	peer->local.handshake = 1;

	/* Temporarily stop processing of all incoming messages while we send
	 * the handshake so that reading of especially the bitfield message
	 * won't cause any interested message to be queued _before_ we have a
	 * chance to send the bitfield message which MUST be first. */
	clear_handlers(peer->socket->fd);

	/* Can't use request_.. version because it will create a new read buffer
	 * and we might want to hold on to the old buffer if the peer ID of the
	 * handshake was not read. */
	write_to_socket(peer->socket, handshake, sizeof(handshake),
			connection_state(S_TRANS),
			sent_bittorrent_peer_handshake);
}

#if 0
/* DHT is not supported, so commented out. */
/* Checks for the DHT flags used by atleast Brams client to indicate it supports
 * trackerless BitTorrent. */
static inline int
bittorrent_peer_supports_dht(unsigned char flags[8])
{
	return !!(flags[7] & 1);
}
#endif

/* This function is called each time there is something from the handshake
 * message to read. */
static enum bittorrent_handshake_state
check_bittorrent_peer_handshake(struct bittorrent_peer_connection *peer,
				struct read_buffer *buffer)
{
	bittorrent_id_T info_hash;
	struct bittorrent_peer *peer_info;
	enum bittorrent_handshake_state state;

	if (buffer->length < 20)
		return BITTORRENT_PEER_HANDSHAKE_INCOMPLETE;

	if (memcmp(buffer->data, BITTORRENT_ID, sizeof(BITTORRENT_ID)))
		return BITTORRENT_PEER_HANDSHAKE_ERROR;

	if (buffer->length < 28)
		return BITTORRENT_PEER_HANDSHAKE_INCOMPLETE;

#if 0
	/* DHT is not supported, so commented out. */
	/* Check the reserved flags */
	peer->remote.dht = bittorrent_peer_supports_dht(&buffer->data[20]);
#endif

	if (buffer->length < 48)
		return BITTORRENT_PEER_HANDSHAKE_INCOMPLETE;

	memcpy(info_hash, &buffer->data[28], sizeof(info_hash));

	if (peer->local.initiater) {
		struct bittorrent_meta *meta = &peer->bittorrent->meta;

		assert(peer->bittorrent);

		/* Check if the info_hash matches the one in the associated
		 * bittorrent connection. */
		if (memcmp(meta->info_hash, info_hash, sizeof(info_hash)))
			return BITTORRENT_PEER_HANDSHAKE_ERROR;

		if (buffer->length < BITTORRENT_PEER_HANDSHAKE_SIZE)
			return BITTORRENT_PEER_HANDSHAKE_INFO_HASH;

		/* If we got the peer info using the compact tracker flag there
		 * is no peer ID, so set it. Else check if the peer has sent the
		 * expected ID. */
		if (bittorrent_id_is_empty(peer->id))
			memcpy(peer->id, &buffer->data[48], sizeof(peer->id));

		else if (memcmp(peer->id, &buffer->data[48], sizeof(peer->id)))
				return BITTORRENT_PEER_HANDSHAKE_ERROR;

	} else {
		struct bittorrent_connection *bittorrent = peer->bittorrent;

		/* If the peer ID is empty we didn't establish the connection. */
		assert(bittorrent_id_is_empty(peer->id));

		/* Look-up the bittorrent connection and drop peer connections
		 * to unknown torrents. */
		if (!bittorrent) {
			bittorrent = find_bittorrent_connection(info_hash);
			if (!bittorrent)
				return BITTORRENT_PEER_HANDSHAKE_ERROR;

			peer->bittorrent = bittorrent;

			/* Don't know if this is the right place to do this
			 * initialization, but it is the first time we know how
			 * many bits there should be room for. Also, it feels
			 * safer to have it created before the peer connection
			 * is moved to a bittorrent connection so all other code
			 * can assume it is always there. */
			peer->bitfield = init_bitfield(bittorrent->meta.pieces);
			if (!peer->bitfield)
				return BITTORRENT_PEER_HANDSHAKE_ERROR;
		}

		/* FIXME: It would be possible to already add the peer to the
		 * neighbor list here. Should we? --jonas */

		if (buffer->length < BITTORRENT_PEER_HANDSHAKE_SIZE)
			return BITTORRENT_PEER_HANDSHAKE_INFO_HASH;

		memcpy(peer->id, &buffer->data[48], sizeof(peer->id));
	}

	assert(peer->bittorrent);

	/* Remove any recorded peer from the peer info list. */
	/* XXX: This has to be done before checking if the peer is known. Since
	 * known in this case means whether the peer already has a connection
	 * associated. */
	peer_info = get_peer_from_bittorrent_pool(peer->bittorrent, peer->id);
	if (peer_info) {
		del_from_list(peer_info);
		mem_free(peer_info);
	}

	/* Even if the peer is already associated with a connection we still
	 * needs to check if the ID is known since we might have just gotten it.
	 * Removing it first makes that possible. */
	del_from_list(peer);

	/* Check if the peer is already connected to us. */
	state = bittorrent_id_is_known(peer->bittorrent, peer->id)
	      ? BITTORRENT_PEER_HANDSHAKE_ERROR : BITTORRENT_PEER_HANDSHAKE_OK;

	/* For unassociated connections; move the peer connection from the list
	 * of unknown pending peer connections to a bittorrent connection's list
	 * of active peers. */
	add_to_list(peer->bittorrent->peers, peer);

	return state;
}

/* Common backend for reading handshakes. */
static enum bittorrent_handshake_state
do_read_bittorrent_peer_handshake(struct socket *socket, struct read_buffer *buffer)
{
	struct bittorrent_peer_connection *peer = socket->conn;
	enum bittorrent_handshake_state state;

	state = check_bittorrent_peer_handshake(peer, buffer);

	switch (state) {
	case BITTORRENT_PEER_HANDSHAKE_OK:
		/* The whole handshake was successfully read. */
		peer->remote.handshake = 1;
		kill_buffer_data(buffer, BITTORRENT_PEER_HANDSHAKE_SIZE);
		update_bittorrent_connection_stats(peer->bittorrent, 0, 0,
						   BITTORRENT_PEER_HANDSHAKE_SIZE);

		switch (get_bittorrent_blacklist_flags(peer->id)) {
		case BITTORRENT_BLACKLIST_PEER_POOL:
		case BITTORRENT_BLACKLIST_NONE:
			break;

		case BITTORRENT_BLACKLIST_MALICIOUS:
		case BITTORRENT_BLACKLIST_BEHAVIOUR:
		default:
			done_bittorrent_peer_connection(peer);
			return BITTORRENT_PEER_HANDSHAKE_ERROR;
		}

		if (!peer->local.handshake)
			send_bittorrent_peer_handshake(socket);
		break;

	case BITTORRENT_PEER_HANDSHAKE_INFO_HASH:
		if (!peer->local.handshake) {
			send_bittorrent_peer_handshake(socket);
			/* XXX: The peer connection might have disappear from
			 * under us at this point so do not reregister the
			 * socket for reading. */
			break;
		}

		read_from_socket(peer->socket, buffer,
				 connection_state(S_TRANS),
				 read_bittorrent_peer_handshake);
		break;

	case BITTORRENT_PEER_HANDSHAKE_ERROR:
		done_bittorrent_peer_connection(peer);
		break;

	case BITTORRENT_PEER_HANDSHAKE_INCOMPLETE:
		/* The whole handshake was not read so wait for more. */
		read_from_socket(peer->socket, buffer,
				 connection_state(S_TRANS),
				 read_bittorrent_peer_handshake);
		break;
	}

	return state;
}

void
read_bittorrent_peer_handshake(struct socket *socket, struct read_buffer *buffer)
{
	do_read_bittorrent_peer_handshake(socket, buffer);
}
