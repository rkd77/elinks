
#ifndef EL__PROTOCOL_BITTORRENT_CONNECTION_H
#define EL__PROTOCOL_BITTORRENT_CONNECTION_H

#include "protocol/protocol.h"

struct bittorrent_connection;
struct connection;

#ifdef CONFIG_BITTORRENT
extern protocol_handler_T bittorrent_protocol_handler;
extern protocol_handler_T bittorrent_peer_protocol_handler;
#else
#define bittorrent_protocol_handler NULL
#define bittorrent_peer_protocol_handler NULL
#endif

void update_bittorrent_connection_state(struct connection *conn);

void
update_bittorrent_connection_stats(struct bittorrent_connection *bittorrent,
				   off_t downloaded, off_t uploaded,
				   off_t received);

void bittorrent_resume_callback(struct bittorrent_connection *bittorrent);

#endif
