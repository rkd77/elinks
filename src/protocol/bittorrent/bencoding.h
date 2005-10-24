
#ifndef EL__PROTOCOL_BITTORRENT_BENCODING_H
#define EL__PROTOCOL_BITTORRENT_BENCODING_H

#include "protocol/bittorrent/common.h"

enum bittorrent_state
parse_bittorrent_metafile(struct bittorrent_meta *meta, struct string *metafile);

enum bittorrent_state
parse_bittorrent_tracker_response(struct bittorrent_connection *bittorrent, struct string *source);

#endif
