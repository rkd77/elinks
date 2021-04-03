
#ifndef EL__PROTOCOL_BITTORRENT_BENCODING_H
#define EL__PROTOCOL_BITTORRENT_BENCODING_H

#include "protocol/bittorrent/common.h"

#ifdef __cplusplus
extern "C" {
#endif

enum bittorrent_state
parse_bittorrent_metafile(struct bittorrent_meta *meta,
			  struct bittorrent_const_string *metafile);

enum bittorrent_state
parse_bittorrent_tracker_response(struct bittorrent_connection *bittorrent,
				  struct bittorrent_const_string *source);

#ifdef __cplusplus
}
#endif

#endif
