/* Library of common BitTorrent code */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "main/select.h"
#include "network/connection.h"
#include "protocol/bittorrent/common.h"
#include "session/download.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/random.h"
#include "util/sha1.h"
#include "util/string.h"
#include "util/snprintf.h"

const bittorrent_id_T BITTORRENT_NULL_ID;

/* Debug function which returns printable peer ID. */
unsigned char *
get_peer_id(bittorrent_id_T peer_id)
{
	static unsigned char hex[41];
	int i, j;

	if (bittorrent_id_is_empty(peer_id)) {
		snprintf(hex, sizeof(hex), "unknown id %p", peer_id);
		return hex;
	}

	for (i = 0, j = 0; i < sizeof(bittorrent_id_T); i++, j++) {
		unsigned char value = peer_id[i];

		if (isprint(value)) {
			hex[j] = value;
		} else {
			hex[j++]   = hx(value >> 4 & 0xF);
			hex[j] = hx(value & 0xF);
		}
	}

	hex[j] = 0;

	return hex;
}

unsigned char *
get_peer_message(enum bittorrent_message_id message_id)
{
	static struct {
		enum bittorrent_message_id message_id;
		unsigned char *name;
	} messages[] = {
		{ BITTORRENT_MESSAGE_INCOMPLETE,	"incomplete"	 },
		{ BITTORRENT_MESSAGE_KEEP_ALIVE,	"keep-alive"	 },
		{ BITTORRENT_MESSAGE_CHOKE,		"choke"		 },
		{ BITTORRENT_MESSAGE_UNCHOKE,		"unchoke"	 },
		{ BITTORRENT_MESSAGE_INTERESTED,	"interested"     },
		{ BITTORRENT_MESSAGE_NOT_INTERESTED,	"not-interested" },
		{ BITTORRENT_MESSAGE_HAVE,		"have"		 },
		{ BITTORRENT_MESSAGE_BITFIELD,		"bitfield"	 },
		{ BITTORRENT_MESSAGE_REQUEST,		"request"	 },
		{ BITTORRENT_MESSAGE_PIECE,		"piece"		 },
		{ BITTORRENT_MESSAGE_CANCEL,		"cancel"	 },

		{ 0, NULL },
	};
	int i;

	for (i = 0; messages[i].name; i++)
		if (messages[i].message_id == message_id)
			return messages[i].name;

	return "unknown";
}


unsigned char *
get_hexed_bittorrent_id(bittorrent_id_T id)
{
	static unsigned char hex[SHA_DIGEST_LENGTH * 2 + 1];
	int i;

	for (i = 0; i < sizeof(bittorrent_id_T); i++) {
		int j = i * 2;

		hex[j++] = hx(id[i] >> 4 & 0xF);
		hex[j]   = hx(id[i] & 0xF);
	}

	hex[SHA_DIGEST_LENGTH * 2] = 0;

	return hex;
}

int
bittorrent_piece_is_valid(struct bittorrent_meta *meta,
			  uint32_t piece, unsigned char *data, uint32_t datalen)
{
	unsigned char *piece_hash;
	bittorrent_id_T data_hash;

	assert(piece < meta->pieces);

	SHA1(data, datalen, data_hash);
	piece_hash = &meta->piece_hash[piece * SHA_DIGEST_LENGTH];

	return !memcmp(data_hash, piece_hash, SHA_DIGEST_LENGTH);
}

void
done_bittorrent_meta(struct bittorrent_meta *meta)
{
	free_uri_list(&meta->tracker_uris);
	mem_free_if(meta->name);
	mem_free_if(meta->comment);
	mem_free_if(meta->piece_hash);
	free_list(meta->files);
}

void
done_bittorrent_message(struct bittorrent_message *message)
{
	del_from_list(message);
	done_uri(message->uri);
	mem_free(message);
}


/* ************************************************************************** */
/* Peer information management: */
/* ************************************************************************** */

/* Generate a peer ID with of the form: 'E' <version> '-' <random> */
void
init_bittorrent_peer_id(bittorrent_id_T peer_id)
{
	unsigned char *version = VERSION;
	int dots = 0;
	int i = 0;

	srand(time(NULL));

	peer_id[i++] = 'E';
	peer_id[i++] = 'L';

	for (; *version && i < sizeof(bittorrent_id_T); version++) {
		if (isdigit(*version)) {
			peer_id[i++] = *version;

		} else if (*version == '.' && !dots) {
			dots = 1;

		} else {
			peer_id[i++] = '-';
			break;
		}

		peer_id[i++] = *version;
	}

	/* Hmm, sizeof(peer_id) don't work here. */
	random_nonce(peer_id + i, sizeof(bittorrent_id_T) - i);
	while (i < sizeof(bittorrent_id_T)) {
		peer_id[i] = hx(peer_id[i] & 0xF);
		i++;
	}
}

int
bittorrent_id_is_known(struct bittorrent_connection *bittorrent,
		       bittorrent_id_T id)
{
	struct bittorrent_peer_connection *peer;

	/* The peer ID matches the client ID? */
	if (!memcmp(bittorrent->peer_id, id, sizeof(bittorrent->peer_id)))
		return 1;

	foreach (peer, bittorrent->peers)
		if (!memcmp(peer->id, id, sizeof(peer->id)))
			return 1;

	if (get_peer_from_bittorrent_pool(bittorrent, id))
		return 1;

	return 0;
}

struct bittorrent_peer *
get_peer_from_bittorrent_pool(struct bittorrent_connection *bittorrent,
			      bittorrent_id_T id)
{
	struct bittorrent_peer *peer_info;

	foreach (peer_info, bittorrent->peer_pool)
		if (!memcmp(peer_info->id, id, sizeof(peer_info->id)))
			return peer_info;

	return NULL;
}

enum bittorrent_state
add_peer_to_bittorrent_pool(struct bittorrent_connection *bittorrent,
			    bittorrent_id_T id, int port,
			    const unsigned char *ip, int iplen)
{
	struct bittorrent_peer *peer;

	/* Check sanity. Don't error out here since entries in the tracker
	 * responses can contain garbage and we don't want to bring down the
	 * whole connection for that. */
	if (iplen <= 0 || !uri_port_is_valid(port))
		return BITTORRENT_STATE_OK;

	/* The ID can be NULL for the compact format. */
	if (id) {
		enum bittorrent_blacklist_flags flags;

		if (bittorrent_id_is_empty(id))
			return BITTORRENT_STATE_ERROR;

		/* Check if the peer is already known. */
		if (bittorrent_id_is_known(bittorrent, id))
			return BITTORRENT_STATE_OK;

		flags = get_bittorrent_blacklist_flags(id);
		if (flags != BITTORRENT_BLACKLIST_NONE)
			return BITTORRENT_STATE_OK;
	}

	/* Really add the peer. */

	peer = mem_calloc(1, sizeof(*peer) + iplen);
	if (!peer) return BITTORRENT_STATE_OUT_OF_MEM;

	peer->port = port;
	if (iplen) memcpy(peer->ip, ip, iplen);
	if (id) memcpy(peer->id, id, sizeof(peer->id));

	add_to_list(bittorrent->peer_pool, peer);

	return BITTORRENT_STATE_OK;
}


/* ************************************************************************** */
/* Peer request management: */
/* ************************************************************************** */

struct bittorrent_peer_request *
get_bittorrent_peer_request(struct bittorrent_peer_status *status,
			    uint32_t piece, uint32_t offset, uint32_t length)
{
	struct bittorrent_peer_request *request;

	foreach (request, status->requests)  {
		if (request->piece == piece
		    && request->offset == offset
		    && request->length == length)
			return request;
	}

	return NULL;
}

void
add_bittorrent_peer_request(struct bittorrent_peer_status *status,
			    uint32_t piece, uint32_t offset, uint32_t length)
{
	struct bittorrent_peer_request *request;

	request = get_bittorrent_peer_request(status, piece, offset, length);
	if (request) return;

	request = mem_alloc(sizeof(*request));
	if (!request) return;

	request->piece	= piece;
	request->offset	= offset;
	request->length	= length;

	/* FIXME: Rather insert the request so that we atleast try to get
	 * some sort of sequential access to piece data. */
	add_to_list_end(status->requests, request);
}

void
del_bittorrent_peer_request(struct bittorrent_peer_status *status,
			    uint32_t piece, uint32_t offset, uint32_t length)
{
	struct bittorrent_peer_request *request;

	request = get_bittorrent_peer_request(status, piece, offset, length);
	if (!request) return;

	del_from_list(request);
	mem_free(request);
}


/* ************************************************************************** */
/* Generic URI downloader: */
/* ************************************************************************** */

struct bittorrent_fetcher {
	struct bittorrent_fetcher **ref;
	bittorrent_fetch_callback_T callback;
	void *data;
	int redirects;
	unsigned int delete:1;
	struct download download;
};

/* This part of the code is very lowlevel and ELinks specific. The download
 * callback is called each time there is some progress, such as new data. So
 * first it basically checks the state and tries to handle it accordingly.
 * Redirects are also handled here. */
static void
bittorrent_fetch_callback(struct download *download, void *data)
{
	struct bittorrent_fetcher *fetcher = data;
	struct fragment *fragment;
	struct bittorrent_const_string response;
	struct cache_entry *cached = download->cached;

	/* If the callback was removed we should shutdown ASAP. */
	if (!fetcher->callback || is_in_state(download->state, S_INTERRUPTED)) {
		if (is_in_state(download->state, S_INTERRUPTED))
			mem_free(fetcher);
		return;
	}

	if (is_in_result_state(download->state)
	    && !is_in_state(download->state, S_OK)) {
		fetcher->callback(fetcher->data, download->state, NULL);
		if (fetcher->ref)
			*fetcher->ref = NULL;
		mem_free(fetcher);
		return;
	}

	if (!cached || is_in_queued_state(download->state))
		return;

	if (cached->redirect && fetcher->redirects++ < MAX_REDIRECTS) {
		cancel_download(download, 0);

		download->state = connection_state(S_WAIT_REDIR);

		load_uri(cached->redirect, cached->uri, download,
			 PRI_DOWNLOAD, CACHE_MODE_NORMAL,
			 download->progress ? download->progress->start : 0);

		return;
	}

	if (is_in_progress_state(download->state))
		return;

	assert(is_in_state(download->state, S_OK));

	/* If the entry is chunked defragment it and grab the single, remaining
	 * fragment. */
	fragment = get_cache_fragment(cached);
	if (!fragment) {
		fetcher->callback(fetcher->data, connection_state(S_OUT_OF_MEM), NULL);
		if (fetcher->ref)
			*fetcher->ref = NULL;
		mem_free(fetcher);
		return;
	}

	response.source = fragment->data;
	response.length = fragment->length;

	fetcher->callback(fetcher->data, connection_state(S_OK), &response);

	if (fetcher->delete)
		delete_cache_entry(cached);
	if (fetcher->ref)
		*fetcher->ref = NULL;
	mem_free(fetcher);
}

struct bittorrent_fetcher *
init_bittorrent_fetch(struct bittorrent_fetcher **fetcher_ref,
		      struct uri *uri, bittorrent_fetch_callback_T callback,
		      void *data, int delete)
{
	struct bittorrent_fetcher *fetcher;

	fetcher = mem_calloc(1, sizeof(*fetcher));
	if (!fetcher) {
		callback(data, connection_state(S_OUT_OF_MEM), NULL);
		return NULL;
	}

	if (fetcher_ref)
		*fetcher_ref = fetcher;

	fetcher->ref		   = fetcher_ref;
	fetcher->callback	   = callback;
	fetcher->data		   = data;
	fetcher->delete		   = delete;
	fetcher->download.callback = bittorrent_fetch_callback;
	fetcher->download.data     = fetcher;

	load_uri(uri, NULL, &fetcher->download, PRI_MAIN, CACHE_MODE_NORMAL, -1);

	return fetcher;
}

static void
end_bittorrent_fetch(void *fetcher_data)
{
	struct bittorrent_fetcher *fetcher = fetcher_data;

	assert(fetcher && !fetcher->callback);

	/* Stop any running connections. */
	cancel_download(&fetcher->download, 0);

	mem_free(fetcher);
}

void
done_bittorrent_fetch(struct bittorrent_fetcher **fetcher_ref)
{
	struct bittorrent_fetcher *fetcher;

	assert(fetcher_ref);

	fetcher = *fetcher_ref;
	*fetcher_ref = NULL;

	assert(fetcher);

	if (fetcher->ref)
		*fetcher->ref = NULL;

	/* Nuke the callback so nothing gets called */
	fetcher->callback = NULL;
	register_bottom_half(end_bittorrent_fetch, fetcher);
}


/* ************************************************************************** */
/* Blacklist management: */
/* ************************************************************************** */

struct bittorrent_blacklist_item {
	LIST_HEAD(struct bittorrent_blacklist_item);

	enum bittorrent_blacklist_flags flags;
	bittorrent_id_T id;
};

static INIT_LIST_OF(struct bittorrent_blacklist_item, bittorrent_blacklist);


static struct bittorrent_blacklist_item *
get_bittorrent_blacklist_item(bittorrent_id_T peer_id)
{
	struct bittorrent_blacklist_item *item;

	foreach (item, bittorrent_blacklist)
		if (!memcmp(item->id, peer_id, sizeof(bittorrent_id_T)))
			return item;

	return NULL;
}

void
add_bittorrent_blacklist_flags(bittorrent_id_T peer_id,
			       enum bittorrent_blacklist_flags flags)
{
	struct bittorrent_blacklist_item *item;

	item = get_bittorrent_blacklist_item(peer_id);
	if (item) {
		item->flags |= flags;
		return;
	}

	item = mem_calloc(1, sizeof(*item));
	if (!item) return;

	item->flags = flags;
	memcpy(item->id, peer_id, sizeof(bittorrent_id_T));

	add_to_list(bittorrent_blacklist, item);
}

void
del_bittorrent_blacklist_flags(bittorrent_id_T peer_id,
			       enum bittorrent_blacklist_flags flags)
{
	struct bittorrent_blacklist_item *item;

	item = get_bittorrent_blacklist_item(peer_id);
	if (!item) return;

	item->flags &= ~flags;
	if (item->flags) return;

	del_from_list(item);
	mem_free(item);
}

enum bittorrent_blacklist_flags
get_bittorrent_blacklist_flags(bittorrent_id_T peer_id)
{
	struct bittorrent_blacklist_item *item;

	item = get_bittorrent_blacklist_item(peer_id);

	return item ? item->flags : BITTORRENT_BLACKLIST_NONE;
}

void
done_bittorrent_blacklist(void)
{
	free_list(bittorrent_blacklist);
}
