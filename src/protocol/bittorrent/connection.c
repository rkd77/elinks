/* Internal "bittorrent" protocol implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "main/timer.h"
#include "network/connection.h"
#include "network/progress.h"
#include "network/socket.h"
#include "protocol/bittorrent/bencoding.h"
#include "protocol/bittorrent/bittorrent.h"
#include "protocol/bittorrent/common.h"
#include "protocol/bittorrent/connection.h"
#include "protocol/bittorrent/tracker.h"
#include "protocol/bittorrent/peerconnect.h"
#include "protocol/bittorrent/peerwire.h"
#include "protocol/bittorrent/piececache.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "session/download.h"
#include "util/bitfield.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"
#include "util/time.h"


/* ************************************************************************** */
/* Peer selection and connection scheduling: */
/* ************************************************************************** */

/* Reschedule updating of the connection state. */
static void
set_bittorrent_connection_timer(struct connection *conn)
{
	struct bittorrent_connection *bittorrent = conn->info;
	milliseconds_T interval = sec_to_ms(get_opt_int("protocol.bittorrent.choke_interval", NULL));

	install_timer(&bittorrent->timer, interval,
		      (void (*)(void *)) update_bittorrent_connection_state,
		      conn);
}

/* Sort the peers based on the stats rate, bubbaly style! */
static void
sort_bittorrent_peer_connections(struct bittorrent_connection *bittorrent)
{
	struct bittorrent_peer_connection *peer, *prev;

	while (1) {
		struct bittorrent_peer_connection *next;
		int resort = 0;

		prev = NULL;

		foreachsafe (peer, next, bittorrent->peers) {
			if (prev && prev->stats.download_rate < peer->stats.download_rate) {
				resort = 1;
				del_from_list(prev);
				add_at_pos(peer, prev);
			}

			prev = peer;
		}

		if (!resort) break;
	};

#ifdef CONFIG_DEBUG
	prev = NULL;
	foreach (peer, bittorrent->peers) {
		assert(!prev || prev->stats.download_rate >= peer->stats.download_rate);
		prev = peer;
	}
#endif
}

/* Timer callback for @bittorrent->timer.  As explained in @install_timer,
 * this function must erase the expired timer ID from all variables.
 *
 * This is basically the choke period handler. */
void
update_bittorrent_connection_state(struct connection *conn)
{
	struct bittorrent_connection *bittorrent = conn->info;
	struct bittorrent_peer_connection *peer, *next_peer;
	int peer_conns, max_peer_conns;
	int min_uploads = get_opt_int("protocol.bittorrent.min_uploads", NULL);
	int max_uploads = get_opt_int("protocol.bittorrent.max_uploads", NULL);

	set_bittorrent_connection_timer(conn);
	/* The expired timer ID has now been erased.  */
	set_connection_timeout(conn);

	peer_conns = list_size(&bittorrent->peers);
	max_peer_conns = get_opt_int("protocol.bittorrent.peerwire.connections", NULL);

	/* First ``age'' the peer rates _before_ the sorting. */
	foreach (peer, bittorrent->peers)
		update_bittorrent_peer_connection_stats(peer, 0, 0, 0);

	/* Sort the peers so that the best peers are at the list start. */
	sort_bittorrent_peer_connections(bittorrent);

	/* Unchoke all the optimal peers. In good spirit, also unchoke all
	 * uninterested peers until the maximum number of interested peers have
	 * been unchoked. The rest is choked. */
	foreachsafe (peer, next_peer, bittorrent->peers) {
		if (!peer->remote.handshake)
			continue;

		if (min_uploads < max_uploads) {
			unchoke_bittorrent_peer(peer);

			/* Uninterested peers are not counted as uploads. */
			if (peer->remote.interested)
				max_uploads--;

		} else {
			choke_bittorrent_peer(peer);
		}

		/* Can remove the peer so we use foreachsafe(). */
		update_bittorrent_peer_connection_state(peer);
	}

	/* FIXME: Find peer(s) to optimistically unchoke. */

	update_bittorrent_piece_cache_state(bittorrent);

	/* Close or open peers connections. */
	if (peer_conns > max_peer_conns) {
		struct bittorrent_peer_connection *prev;

		foreachsafe (peer, prev, bittorrent->peers) {
			done_bittorrent_peer_connection(peer);
			if (--peer_conns <= max_peer_conns)
				break;
		}

	} else if (peer_conns < max_peer_conns) {
		struct bittorrent_peer *peer_info, *next_peer_info;

		foreachsafe (peer_info, next_peer_info, bittorrent->peer_pool) {
			enum bittorrent_state state;

			state = make_bittorrent_peer_connection(bittorrent, peer_info);
			if (state != BITTORRENT_STATE_OK)
				break;

			del_from_list(peer_info);
			mem_free(peer_info);
			if (++peer_conns >= max_peer_conns)
				break;
		}
	}

	assert(peer_conns <= max_peer_conns);

	/* Shrink the peer pool. */
	if (!list_empty(bittorrent->peers)) {
		struct bittorrent_peer *peer_info, *next_peer_info;
		int pool_size = get_opt_int("protocol.bittorrent.peerwire.pool_size", NULL);
		int pool_peers = 0;

		foreachsafe (peer_info, next_peer_info, bittorrent->peer_pool) {
			/* Unlimited. */
			if (!pool_size) break;

			if (pool_peers < pool_size) {
				pool_peers++;
				continue;
			}

			del_from_list(peer_info);
			mem_free(peer_info);
		}
	}
}

/* Progress timer callback for @bittorrent->upload_progress.  */
static void
update_bittorrent_connection_upload(void *data)
{
	struct bittorrent_connection *bittorrent = data;

	update_progress(&bittorrent->upload_progress,
			bittorrent->uploaded,
			bittorrent->downloaded,
			bittorrent->uploaded);
}

void
update_bittorrent_connection_stats(struct bittorrent_connection *bittorrent,
				   off_t downloaded, off_t uploaded,
				   off_t received)
{
	struct bittorrent_meta *meta = &bittorrent->meta;

	if (bittorrent->conn->est_length == -1) {
		off_t length = (off_t) (meta->pieces - 1) * meta->piece_length
			     + meta->last_piece_length;

		bittorrent->conn->est_length = length;
		bittorrent->left = length;
		start_update_progress(&bittorrent->upload_progress,
				      update_bittorrent_connection_upload,
				      bittorrent);
	}

	if (bittorrent->upload_progress.timer == TIMER_ID_UNDEF
	    && bittorrent->uploaded)
		update_bittorrent_connection_upload(bittorrent);

	bittorrent->conn->received += received;
	bittorrent->conn->from	   += downloaded;
	if (downloaded > 0)
		bittorrent->downloaded	   += downloaded;
	bittorrent->uploaded	   += uploaded;
	bittorrent->left	   -= downloaded;

	if (!bittorrent->downloaded) return;

	bittorrent->sharing_rate    = (double) bittorrent->uploaded
					     / bittorrent->downloaded;
}


/* ************************************************************************** */
/* The ``main'' BitTorrent connection setup: */
/* ************************************************************************** */

/* Callback which is attached to the ELinks connection and invoked when the
 * connection is shutdown. */
static void
done_bittorrent_connection(struct connection *conn)
{
	struct bittorrent_connection *bittorrent = conn->info;
	struct bittorrent_peer_connection *peer, *next;

	assert(bittorrent);
	assert(conn->done == done_bittorrent_connection);

	/* We don't want the tracker to see the fetch. */
	if (bittorrent->fetch)
		done_bittorrent_fetch(&bittorrent->fetch);

	foreachsafe (peer, next, bittorrent->peers)
		done_bittorrent_peer_connection(peer);

	done_bittorrent_tracker_connection(conn);
	done_bittorrent_listening_socket(conn);
	if (bittorrent->cache)
		done_bittorrent_piece_cache(bittorrent);
	done_bittorrent_meta(&bittorrent->meta);

	kill_timer(&bittorrent->timer);
	kill_timer(&bittorrent->upload_progress.timer);

	free_list(bittorrent->peer_pool);

	mem_free_set(&conn->info, NULL);
	conn->done = NULL;
}

static struct bittorrent_connection *
init_bittorrent_connection(struct connection *conn)
{
	struct bittorrent_connection *bittorrent;

	assert(conn->info == NULL);
	assert(conn->done == NULL);
	if_assert_failed return NULL;

	bittorrent = mem_calloc(1, sizeof(*bittorrent));
	if (!bittorrent) return NULL;

	init_list(bittorrent->peers);
	init_list(bittorrent->peer_pool);

	/* conn->info and conn->done were asserted as NULL above.  */
	conn->info = bittorrent;
	conn->done = done_bittorrent_connection;

	init_bittorrent_peer_id(bittorrent->peer_id);

	bittorrent->conn = conn;
	bittorrent->tracker.timer = TIMER_ID_UNDEF;

	/* Initialize here so that error handling can safely call
	 * free_list on it. */
	init_list(bittorrent->meta.files);

	return bittorrent;
}

void
bittorrent_resume_callback(struct bittorrent_connection *bittorrent)
{
	struct connection_state state;

	/* Failing to create the listening socket is fatal. */
	state = init_bittorrent_listening_socket(bittorrent->conn);
	if (!is_in_state(state, S_OK)) {
		retry_connection(bittorrent->conn, state);
		return;
	}

	set_connection_state(bittorrent->conn, connection_state(S_CONN_TRACKER));
	send_bittorrent_tracker_request(bittorrent->conn);
}

/* Metainfo file download callback */
static void
bittorrent_metainfo_callback(void *data, struct connection_state state,
			     struct bittorrent_const_string *response)
{
	struct connection *conn = data;
	struct bittorrent_connection *bittorrent = conn->info;

	bittorrent->fetch = NULL;

	if (!is_in_state(state, S_OK)) {
		abort_connection(conn, state);
		return;
	}

	switch (parse_bittorrent_metafile(&bittorrent->meta, response)) {
	case BITTORRENT_STATE_OK:
	{
		size_t size = list_size(&bittorrent->meta.files);
		int *selection;

		assert(bittorrent->tracker.event == BITTORRENT_EVENT_STARTED);

		selection = get_bittorrent_selection(conn->uri, size);
		if (selection) {
			struct bittorrent_file *file;
			int index = 0;

			foreach (file, bittorrent->meta.files)
				file->selected = selection[index++];

			mem_free(selection);
		}

		switch (init_bittorrent_piece_cache(bittorrent, response)) {
		case BITTORRENT_STATE_OK:
			bittorrent_resume_callback(bittorrent);
			return;

		case BITTORRENT_STATE_CACHE_RESUME:
			set_connection_state(bittorrent->conn,
					     connection_state(S_RESUME));
			return;

		case BITTORRENT_STATE_OUT_OF_MEM:
			state = connection_state(S_OUT_OF_MEM);
			break;

		default:
			state = connection_state(S_BITTORRENT_ERROR);
		}

		break;
	}
	case BITTORRENT_STATE_OUT_OF_MEM:
		state = connection_state(S_OUT_OF_MEM);
		break;

	case BITTORRENT_STATE_ERROR:
	default:
		/* XXX: This can also happen when passing bittorrent:<uri> and
		 * <uri> gives an HTTP 404 response. It might be worth fixing by
		 * looking at the protocol header, however, direct usage of the
		 * internal bittorrent: is at your own risk ... at least for
		 * now. --jonas */
		state = connection_state(S_BITTORRENT_METAINFO);
	}

	abort_connection(conn, state);
}

/* The entry point for BitTorrent downloads. */
void
bittorrent_protocol_handler(struct connection *conn)
{
	struct uri *uri = NULL;
	struct bittorrent_connection *bittorrent;

	bittorrent = init_bittorrent_connection(conn);
	if (!bittorrent) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	if (conn->uri->datalen)
		uri = get_uri(conn->uri->data, 0);

	if (!uri) {
		abort_connection(conn, connection_state(S_BITTORRENT_BAD_URL));
		return;
	}

	set_connection_state(conn, connection_state(S_CONN));
	set_connection_timeout(conn);
	conn->from = 0;

	init_bittorrent_fetch(&bittorrent->fetch, uri,
			      bittorrent_metainfo_callback, conn, 0);
	done_uri(uri);
}

void
bittorrent_peer_protocol_handler(struct connection *conn)
{
	abort_connection(conn, connection_state(S_BITTORRENT_PEER_URL));
}
