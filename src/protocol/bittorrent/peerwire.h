
#ifndef EL__PROTOCOL_BITTORRENT_PEERWIRE_H
#define EL__PROTOCOL_BITTORRENT_PEERWIRE_H

#include "protocol/bittorrent/common.h"
#include "util/lists.h"

struct read_buffer;
struct socket;

void read_bittorrent_peer_handshake(struct socket *socket, struct read_buffer *buffer);
void send_bittorrent_peer_handshake(struct socket *socket);

void
update_bittorrent_peer_connection_state(struct bittorrent_peer_connection *peer);

void
update_bittorrent_peer_connection_stats(struct bittorrent_peer_connection *peer,
					uint32_t downloaded, uint32_t have_piece,
					uint32_t uploaded);

void
send_bittorrent_peer_message(struct bittorrent_peer_connection *peer,
			     enum bittorrent_message_id message_id, ...);

static void inline
set_bittorrent_peer_interested(struct bittorrent_peer_connection *peer)
{
	if (peer->local.interested) return;
	peer->local.interested = 1;
	send_bittorrent_peer_message(peer, BITTORRENT_MESSAGE_INTERESTED);
}

static void inline
set_bittorrent_peer_not_interested(struct bittorrent_peer_connection *peer)
{
	if (!peer->local.interested) return;
	peer->local.interested = 0;
	send_bittorrent_peer_message(peer, BITTORRENT_MESSAGE_NOT_INTERESTED);
}

static void inline
choke_bittorrent_peer(struct bittorrent_peer_connection *peer)
{
	if (peer->remote.choked) return;
	peer->remote.choked = 1;
	send_bittorrent_peer_message(peer, BITTORRENT_MESSAGE_CHOKE);
	free_list(peer->remote.requests);
}

static void inline
unchoke_bittorrent_peer(struct bittorrent_peer_connection *peer)
{
	if (!peer->remote.choked) return;
	peer->remote.choked = 0;
	send_bittorrent_peer_message(peer, BITTORRENT_MESSAGE_UNCHOKE);
}

static void inline
set_bittorrent_peer_have(struct bittorrent_peer_connection *peer, uint32_t piece)
{
	struct bittorrent_connection *bittorrent = peer->bittorrent;

	foreach (peer, bittorrent->peers) {
		/* If the bitfield hasn't been sent there is no need to send a
		 * have message. */
		if (!peer->local.bitfield)
			continue;

		send_bittorrent_peer_message(peer, BITTORRENT_MESSAGE_HAVE, piece);
	}
}

static void inline
cancel_bittorrent_peer_request(struct bittorrent_peer_connection *peer,
			       struct bittorrent_peer_request *request)
{
	send_bittorrent_peer_message(peer, BITTORRENT_MESSAGE_CANCEL,
				     request->piece, request->offset,
				     request->length);
}

#endif
