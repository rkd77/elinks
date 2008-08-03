
#ifndef EL__PROTOCOL_BITTORRENT_PEERCONNECT_H
#define EL__PROTOCOL_BITTORRENT_PEERCONNECT_H

#include "network/state.h"
#include "protocol/bittorrent/common.h"

struct connection;

/* Sets up and tears down the peer listening socket. */
struct connection_state init_bittorrent_listening_socket(struct connection *conn);
void done_bittorrent_listening_socket(struct connection *conn);

void done_bittorrent_peer_connection(struct bittorrent_peer_connection *peer);
void set_bittorrent_peer_connection_timeout(struct bittorrent_peer_connection *peer);

struct bittorrent_connection *find_bittorrent_connection(bittorrent_id_T info_hash);

enum bittorrent_state
make_bittorrent_peer_connection(struct bittorrent_connection *bittorrent,
				struct bittorrent_peer *peer_info);

#endif
