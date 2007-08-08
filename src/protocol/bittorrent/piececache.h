
#ifndef EL__PROTOCOL_BITTORRENT_PIECECACHE_H
#define EL__PROTOCOL_BITTORRENT_PIECECACHE_H

#include "protocol/bittorrent/common.h"
#include "util/lists.h"

struct bitfield;
struct string;

struct bittorrent_piece_cache_entry {
	LIST_HEAD(struct bittorrent_piece_cache_entry);

	/** Piece rarity index
	 * To keep track of the client's view of the swarm in regards to pieces a
	 * piece rarity index for neighboring peers is maintained for each piece
	 * in the torrent. It keeps track of how many neighboring peers have the
	 * piece. The smaller the value the more rare the piece is. The table is
	 * updated when the client receives bitfield or have messages. Zero
	 * indicates that no neightboring peer has the piece. */
	uint16_t rarity;

	unsigned int completed:1;	/**< All blocks was downloaded. */
	unsigned int remaining:1;	/**< Nothing has been even requested. */
	unsigned int locked:1;		/**< Edge piece from partial downloads. */
	unsigned int selected:1;	/**< Piece is part of partial download. */

	/** A bitfield of the blocks which remains to be downloaded for this
	 * piece. May be NULL if downloading is not in progress. */
	struct bitfield *blocks;

	/** The data of the piece.
	 * May be NULL if data has not been downloaded
	 * or the piece has been written to disk.
	 * XXX: This memory is mmaped using the mem_mmap_*() functions. */
	unsigned char *data;
};

struct bittorrent_piece_cache {
	/* The following is mostly maintained for making it easy to display in
	 * dialogs. */
	unsigned int remaining_pieces;	/**< Number of untouched pieces */
	unsigned int completed_pieces;	/**< Number of downloaded pieces */
	unsigned int loading_pieces;	/**< Number of pieces in progress */
	unsigned int rejected_pieces;	/**< Number of hash check rejects */
	unsigned int unavailable_pieces;/**< Number of unavailable pieces */
	unsigned int partial_pieces;	/**< Number of selected file pieces */
	unsigned int locked_pieces;	/**< Pieces locked due to partial download */

	/* Flags set from the download dialog. */
	unsigned int delete_files:1;	/**< Unlink files on shutdown? */
	unsigned int notify_complete:1;	/**< Notify upon completion? */
	unsigned int partial:1;		/**< Dealing with a partial download? */

	/** The pipe descripter used for communicating with the resume thread. */
	int resume_fd;
	uint32_t resume_pos;

	/** A bitfield of the available pieces. */
	struct bitfield *bitfield;

	/** A list of completed and saved entries which has been loaded into
	 * memory. The allocated memory for all these entries is disposable. The
	 * entries are sorted in a LRU-manner. */
	LIST_OF(struct bittorrent_piece_cache_entry) queue;

	/** Remaining pieces are tracked using the remaining_blocks member of the
	 * piece cache entry and a free list of piece blocks to be requested.
	 * Requests are taken from the free list every time a peer queries which
	 * piece block to request next. If the piece list is empty (or if the
	 * remote peer does not have any of the pieces currently in the free
	 * list) the cache is searched for a remaining piece and any found piece
	 * is then broken into a number of requests. Typically, the querying
	 * peer will only add a few of the requests to its queue so the rest of
	 * the requests will end up in the free list. This way more than one
	 * peer can request blocks from the same piece and the overall strategy
	 * will try to finish already started pieces before it begins
	 * downloading of new ones.
	 *
	 * When the free list is empty _and_ the number of remaining pieces is
	 * zero the client enters the end game mode. That is, _all_ remaining
	 * requests are _pending_. To speed up the end game mode, more than one
	 * peer is allowed to request the same piece, however, it is important
	 * to cancel these requests when they are completed to not waste
	 * bandwitdth. To ease canceling in end game mode peer requests has a
	 * cloned flag. The flag is set when piece block requests are cloned. If
	 * the cloned flag is set when receiving a block then the peer-list is
	 * searched and requests for the same piece is canceled. */
	LIST_OF(struct bittorrent_piece_request) free_list;
	struct bittorrent_piece_cache_entry entries[1];
};

enum bittorrent_state
init_bittorrent_piece_cache(struct bittorrent_connection *bittorrent,
			    struct string *metafile);

void done_bittorrent_piece_cache(struct bittorrent_connection *bittorrent);

void update_bittorrent_piece_cache_state(struct bittorrent_connection *bittorrent);

/* Depending on the current download mode, try to find a block to request. */
struct bittorrent_peer_request *
find_bittorrent_peer_request(struct bittorrent_peer_connection *peer);

void
add_requests_to_bittorrent_piece_cache(struct bittorrent_peer_connection *peer,
				       struct bittorrent_peer_status *status);

void
update_bittorrent_piece_cache(struct bittorrent_peer_connection *peer,
			      uint32_t piece);

void
update_bittorrent_piece_cache_from_bitfield(struct bittorrent_peer_connection *peer);

void
remove_bittorrent_peer_from_piece_cache(struct bittorrent_peer_connection *peer);

enum bittorrent_state
add_to_bittorrent_piece_cache(struct bittorrent_peer_connection *peer,
			      uint32_t piece, uint32_t offset,
			      unsigned char *data, uint32_t datalen,
			      int *write_errno);

unsigned char *
get_bittorrent_piece_cache_data(struct bittorrent_connection *bittorrent,
				uint32_t piece);

#endif
