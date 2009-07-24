
#ifndef EL__PROTOCOL_BITTORRENT_COMMON_H
#define EL__PROTOCOL_BITTORRENT_COMMON_H

#include "main/timer.h"
#include "network/progress.h"
#include "network/socket.h"
#include "network/state.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/md5.h"
#include "util/sha1.h"
#include "util/time.h"

struct bitfield;
struct bittorrent_fetcher;
struct connection;
struct terminal;

/** The least acceptable default sharing rate. */
#define BITTORRENT_DEFAULT_SHARING_RATE		0.250

/** The number of seconds between updating the connection state and most
 * importantly choke and unchoke peer connections. */
#define BITTORRENT_DEFAULT_CHOKE_INTERVAL	10

/** The length regarded as ``typical'' by the community wiki specification.
 * Looks like Bram uses 2^14 here.
 * Used for the protocol.bittorrent.request_length option */
#define BITTORRENT_REQUEST_LENGTH		(1 << 14)

/** The length of requested blocks of pieces should not exceed 2^17 bytes.
 * Used for the protocol.bittorrent.max_request_length option.
 * Bram uses 2^23 here. */
#define BITTORRENT_REQUEST_ACCEPT_LENGTH	(1 << 23)

/** The maximum size to allow a peer message to have. */
/* Bram uses 2^23 here. */
#define BITTORRENT_MESSAGE_MAX_SIZE		(1 << 23)


/** 20-byte string ID used for both peer IDs and info-hashes. */
typedef sha1_digest_bin_T bittorrent_id_T;

/** Special peer ID used for determining whether an ID has been set. */
extern const bittorrent_id_T BITTORRENT_NULL_ID;

/** Check if the ID has been set. */
#define bittorrent_id_is_empty(id) \
	(!memcmp(id, BITTORRENT_NULL_ID, sizeof(bittorrent_id_T)))


/** BitTorrent error states. */
enum bittorrent_state {
	BITTORRENT_STATE_OK,			/**< All is well. */
	BITTORRENT_STATE_ERROR,			/**< Some error happened. */
	BITTORRENT_STATE_REQUEST_FAILURE,	/**< Failure from tracker.  */
	BITTORRENT_STATE_OUT_OF_MEM,		/**< Allocation failure. */
	BITTORRENT_STATE_CACHE_FAILURE,		/**< Cache data access failed. */
	BITTORRENT_STATE_CACHE_RESUME,		/**< Resume state from disk. */
	BITTORRENT_STATE_FILE_MISSING,		/**< File does not exist. */
};

/** For showing tracker failure responses to the user. */
struct bittorrent_message {
	LIST_HEAD(struct bittorrent_message);

	struct uri *uri;
	struct connection_state state;
	unsigned char string[1];
};


/* ************************************************************************** */
/* Peer-wire types: */
/* ************************************************************************** */

/** BitTorrent peer-wire state and message IDs. */
enum bittorrent_message_id {
	/* Special internal state and message type. */
	BITTORRENT_MESSAGE_ERROR		= -3,
	BITTORRENT_MESSAGE_INCOMPLETE		= -2,
	BITTORRENT_MESSAGE_KEEP_ALIVE		= -1,

	/* Valid message types. */
	BITTORRENT_MESSAGE_CHOKE		=  0,
	BITTORRENT_MESSAGE_UNCHOKE		=  1,
	BITTORRENT_MESSAGE_INTERESTED		=  2,
	BITTORRENT_MESSAGE_NOT_INTERESTED	=  3,
	BITTORRENT_MESSAGE_HAVE			=  4,
	BITTORRENT_MESSAGE_BITFIELD		=  5,
	BITTORRENT_MESSAGE_REQUEST		=  6,
	BITTORRENT_MESSAGE_PIECE		=  7,
	BITTORRENT_MESSAGE_CANCEL		=  8,
};

/** The peer request matches information sent in the request and cancel messages
 * in the peer-wire protocol. See the piece cache header file (cache.h) for more
 * information about the cloned flag. */
struct bittorrent_peer_request {
	LIST_HEAD(struct bittorrent_peer_request);

	uint32_t piece;	 		/**< Zero-based piece index. */
	uint32_t offset; 		/**< Zero-based piece byte offset. */
	uint32_t length;		/**< The wanted number of bytes. */

	uint16_t block;			/**< The block index in the piece. */

	enum bittorrent_message_id id;	/**< ID of queued pending message. */

	unsigned int cloned:1;		/**< The request was cloned. */
	unsigned int requested:1;	/**< Whether it has been requested. */
};

struct bittorrent_peer_status {
	/** FIFO-like recording of requests. */
	LIST_OF(struct bittorrent_peer_request) requests;

	/* Flags for scheduling updating of the peer state. */
	unsigned int choked:1;		/**< The peer was choked. */
	unsigned int interested:1;	/**< The peer is interested. */
	unsigned int snubbed:1;		/**< The peer was snubbed. */

	/* State flags used for determining what to accept. */
	unsigned int handshake:1;	/**< The handshake was sent. */
	unsigned int bitfield:1;	/**< The bitfield was sent. */
	unsigned int initiater:1;	/**< Initiater of the connection. */
	unsigned int seeder:1;		/**< The peer has the complete torrent. */
};

struct bittorrent_peer_stats {
	time_t last_time;
	time_t age;

	double download_rate;
	double have_rate;

	off_t downloaded;
	off_t uploaded;
};

/** Peer connection information. */
struct bittorrent_peer_connection {
	LIST_HEAD(struct bittorrent_peer_connection);

	/** Unique peer ID string which can be used to look-up the peer hash. */
	bittorrent_id_T	id;

	/** Timer handle for scheduling timeouts. */
	timer_id_T timer;

	/** Socket information. */
	struct socket *socket;

	/** Progress information and counter for the number of uploaded or
	 * downloaded bytes depending on the mode. */
	struct bittorrent_peer_stats stats;

	/** The BitTorrent connection the peer connection is associated with.
	 * For recently accepted peer connections it might be NULL indicating
	 * that the info_hash has not yet been read from the handshake. */
	struct bittorrent_connection *bittorrent;

	/** Local client and remote peer status info. */
	struct bittorrent_peer_status local;
	struct bittorrent_peer_status remote;

	/** Outgoing message queue. Note piece messages are maintained entirely
	 * in the request list in the bittorrent_peer_status struct. */
	LIST_OF(struct bittorrent_peer_request) queue;

	/** A bitfield of the available pieces from the peer.
	 * The size depends on the number of pieces. */
	struct bitfield *bitfield;
};


/* ************************************************************************** */
/* Tracker types: */
/* ************************************************************************** */

/** Event state information needed by the tracker. */
enum bittorrent_tracker_event {
	BITTORRENT_EVENT_STARTED = 0,	/**< XXX: Zero, to always send first */
	BITTORRENT_EVENT_STOPPED,	/**< Graceful shut down  */
	BITTORRENT_EVENT_COMPLETED,	/**< Download was completed */
	BITTORRENT_EVENT_REGULAR,	/**< Regular (periodical) tracker request */
};

/** This stores info about tracker requests.
 * It is not a real connection because it consists of a series of HTTP requests
 * but both the tracker and client is supposed to keep state information across
 * all requests. */
struct bittorrent_tracker_connection {
	/** Used for keeping track of when to send event info to the tracker. */
	enum bittorrent_tracker_event event;

	/** Time in seconds between contacting the tracker and a timer handle. */
	timer_id_T timer;
	int interval;

	/** Requesting the tracker failed or was never started so no
	 * event=stopped should be sent. */
	unsigned int failed:1;
	unsigned int started:1;
};


/* ************************************************************************** */
/* Metafile types: */
/* ************************************************************************** */

/** Information about peers returned by the tracker. */
struct bittorrent_peer {
	LIST_HEAD(struct bittorrent_peer);

	bittorrent_id_T	id;		/**< Unique peer ID string. */
	uint16_t	port;		/**< The port number to connect to. */
	unsigned char	ip[1];		/**< String with a IPv4 or IPv6 address. */
};

/** Information about a file in the torrent. */
struct bittorrent_file {
	LIST_HEAD(struct bittorrent_file);

	off_t		 length;	/**< Length of the file in bytes. */
	md5_digest_hex_T md5sum;	/**< Hexadecimal MD5 sum of the file. */
	int		 selected;
	unsigned char    name[1];	/**< Filename converted from path list. */
};

/** Static information from the .torrent metafile. */
struct bittorrent_meta {
	/** The SHA1 info hash of the value of the info key from the metainfo
	 * .torrent file is used regularly when connecting to both the tracker
	 * and peers. */
	bittorrent_id_T info_hash;

	/** Optional information about the creation time of the torrent.
	 * Used if the document.download.set_original_time is true. */
	time_t creation_date;

	/** Optional comment in free-form text. */
	unsigned char *comment;

	/** The announced URI of each available tracker. */
	struct uri_list tracker_uris;

	/** The number of pieces. */
	uint32_t pieces;

	/** The number of bytes in each piece. */
	uint32_t piece_length;
	/** The last piece can be shorter than the others. */
	uint32_t last_piece_length;

	/** List of concatenated SHA1 hash values for each piece. */
	unsigned char *piece_hash;

	/** The type of the torrent. */
	enum { BITTORRENT_SINGLE_FILE, BITTORRENT_MULTI_FILE } type;

	/** Potential bad file path detected. */
	unsigned int malicious_paths:1;

	/** The name of either the single file or the top-most directory. */
	unsigned char *name;

	/** A list with information about files in the torrent.
	 * The list is a singleton for single-file torrents. */
	LIST_OF(struct bittorrent_file) files;
};

enum bittorrent_connection_mode {
	BITTORRENT_MODE_PIECELESS,	/**< The client has no piece to share. */
	BITTORRENT_MODE_NORMAL,		/**< The client is up- and downloading. */
	BITTORRENT_MODE_END_GAME,	/**< All remaining pieces are requested. */
	BITTORRENT_MODE_SEEDER,		/**< The client is only uploading. */
};

/** This stores info about an active BitTorrent connection. Note, the list head
 * is used by the handling of the peer-wire listening socket and should only be
 * managed by that. */
struct bittorrent_connection {
	LIST_HEAD(struct bittorrent_connection);

	enum bittorrent_connection_mode mode;

	/** Static information from the .torrent metafile. */
	struct bittorrent_meta meta;

	/** Dynamic tracker information. */
	struct bittorrent_tracker_connection tracker;

	/** Dynamic tracker information. */
	struct bittorrent_piece_cache *cache;

	/** Back-reference to the connection the bittorrent connection belongs
	 * to. */
	struct connection *conn;

	/** Active peer list
	 * The size is controlled by the protocol.bittorrent.max_active_peers
	 * option. */
	LIST_OF(struct bittorrent_peer_connection) peers;

	/** List of information about potential peers.
	 * @todo TODO: Use hash. */
	LIST_OF(struct bittorrent_peer) peer_pool;

	/** The peer ID of the client. */
	bittorrent_id_T peer_id;

	/** The port of the listening socket */
	uint16_t port;

	/** Timer handle for scheduling periodic updating and rating of peer
	 * connections. */
	timer_id_T timer;

	/** Statistics for the tracker and total progress information for the
	 * user interface. */
	struct progress upload_progress;
	off_t uploaded;
	off_t downloaded;
	off_t left;

	/** Number of seeders. */
	uint32_t complete;
	/** Number of leechers. */
	uint32_t incomplete;

	double sharing_rate;

	/** Information about any running metainfo file or tracker request. */
	struct bittorrent_fetcher *fetch;

	/** For notifying on completion.
	 * May be NULL. */
	struct terminal *term;
};

/** Like struct string, except the data is const and not freed via
 * this structure.  So it is okay to make @c source point to data that
 * is part of a larger buffer.  Also, there is no @c magic member here.  */
struct bittorrent_const_string {
	const unsigned char *source;
	int length;
};

static inline uint32_t
get_bittorrent_piece_length(struct bittorrent_meta *meta, uint32_t piece)
{
	return piece == meta->pieces - 1
		? meta->last_piece_length : meta->piece_length;
}

unsigned char *get_hexed_bittorrent_id(bittorrent_id_T id);


int
bittorrent_piece_is_valid(struct bittorrent_meta *meta,
			  uint32_t piece, unsigned char *data, uint32_t datalen);

void init_bittorrent_peer_id(bittorrent_id_T peer_id);

int
bittorrent_id_is_known(struct bittorrent_connection *bittorrent,
		       bittorrent_id_T id);

enum bittorrent_state
add_peer_to_bittorrent_pool(struct bittorrent_connection *bittorrent,
			    bittorrent_id_T id, int port,
			    const unsigned char *ip, int iplen);

struct bittorrent_peer *
get_peer_from_bittorrent_pool(struct bittorrent_connection *bittorrent,
			      bittorrent_id_T id);

void done_bittorrent_meta(struct bittorrent_meta *meta);
void done_bittorrent_message(struct bittorrent_message *message);


/* ************************************************************************** */
/* Debug 'pretty printing' functions: */
/* ************************************************************************** */

unsigned char *get_peer_id(bittorrent_id_T peer);
unsigned char *get_peer_message(enum bittorrent_message_id message_id);

/* ************************************************************************** */
/* Peer request management: */
/* ************************************************************************** */

struct bittorrent_peer_request *
get_bittorrent_peer_request(struct bittorrent_peer_status *status,
			    uint32_t piece, uint32_t offset, uint32_t length);

void
add_bittorrent_peer_request(struct bittorrent_peer_status *status,
			    uint32_t piece, uint32_t offset, uint32_t length);

void
del_bittorrent_peer_request(struct bittorrent_peer_status *status,
			    uint32_t piece, uint32_t offset, uint32_t length);


/* ************************************************************************** */
/* URI fetching: */
/* ************************************************************************** */

typedef void (*bittorrent_fetch_callback_T)(void *, struct connection_state,
					    struct bittorrent_const_string *);

struct bittorrent_fetcher *
init_bittorrent_fetch(struct bittorrent_fetcher **fetcher_ref,
		      struct uri *uri, bittorrent_fetch_callback_T callback,
		      void *data, int delete);
void done_bittorrent_fetch(struct bittorrent_fetcher **fetcher_ref);


/* ************************************************************************** */
/* Blacklisting: */
/* ************************************************************************** */

enum bittorrent_blacklist_flags {
	BITTORRENT_BLACKLIST_NONE,	/**< No blacklisting is in effect */
	BITTORRENT_BLACKLIST_PEER_POOL,	/**< Blacklist from peer pool. */
	BITTORRENT_BLACKLIST_MALICIOUS,	/**< Malicious peer, refuse connection */
	BITTORRENT_BLACKLIST_BEHAVIOUR,	/**< Unfair behaviour, refuse connection */
};

void
add_bittorrent_blacklist_flags(bittorrent_id_T peer_id,
			       enum bittorrent_blacklist_flags flags);

void
del_bittorrent_blacklist_flags(bittorrent_id_T peer_id,
			       enum bittorrent_blacklist_flags flags);

enum bittorrent_blacklist_flags
get_bittorrent_blacklist_flags(bittorrent_id_T peer_id);

void done_bittorrent_blacklist(void);

#endif
