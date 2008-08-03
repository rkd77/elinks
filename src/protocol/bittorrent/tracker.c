/* BitTorrent tracker HTTP protocol implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "main/select.h"
#include "main/timer.h"
#include "network/connection.h"
#include "network/socket.h"
#include "protocol/bittorrent/bencoding.h"
#include "protocol/bittorrent/bittorrent.h"
#include "protocol/bittorrent/common.h"
#include "protocol/bittorrent/connection.h"
#include "protocol/bittorrent/tracker.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "util/memory.h"
#include "util/string.h"


/* Y'all bases iz belong ta us. */

/* TODO: Support for the scrape convention. It should be optional through an
 * update interval option indicating in seconds how often to request the scrape
 * info. Zero means off. */

struct uri_list bittorrent_stopped_requests;

static void do_send_bittorrent_tracker_request(struct connection *conn);

static void
set_bittorrent_tracker_interval(struct connection *conn)
{
	struct bittorrent_connection *bittorrent = conn->info;
	int interval = get_opt_int("protocol.bittorrent.tracker.interval");

	/* The default HTTP_KEEPALIVE_TIMEOUT is set to 60 seconds so it
	 * probably makes sense to have a default tracker interval that is below
	 * that limit to get good performance. On the other hand it is probably
	 * also good to be nice and follow the trackers intructions. Rate limit
	 * the interval to be minimum twice each minute so it doesn't end up
	 * hogging the CPU too much. */
	if (!interval)
		interval = int_min(bittorrent->tracker.interval, 60 * 5);

	if (interval <= 0)
		interval = 60 * 5;

	install_timer(&bittorrent->tracker.timer, sec_to_ms(interval),
		      (void (*)(void *)) do_send_bittorrent_tracker_request,
		      conn);
}

/* Handling of the regular HTTP request/response connnection to the tracker, */
/* XXX: The data pointer may be NULL when doing event=stopped because no
 * connection is attached anymore. */
static void
bittorrent_tracker_callback(void *data, struct connection_state state,
			    struct string *response)
{
	struct connection *conn = data;
	struct bittorrent_connection *bittorrent = conn ? conn->info : NULL;

	/* We just did event=stopped and don't have any connection attached. */
	if (!bittorrent) return;

	assert(bittorrent->tracker.event != BITTORRENT_EVENT_STOPPED);

	bittorrent->fetch = NULL;

	/* FIXME: We treat any error as fatal here, however, it might be better
	 * to relax that and allow a few errors before ending the connection. */
	if (!is_in_state(state, S_OK)) {
		if (is_in_state(state, S_INTERRUPTED))
			return;
		bittorrent->tracker.failed = 1;
		add_bittorrent_message(conn->uri, state, NULL);
		abort_connection(conn, connection_state(S_OK));
		return;
	}

	switch (parse_bittorrent_tracker_response(bittorrent, response)) {
	case BITTORRENT_STATE_ERROR:
		/* This error means a parsing error, and it actually seems to
		 * happen so frequently that they are worth ignoring from a
		 * usability perspective, e.g. they may be caused by the peer
		 * info list suddenly ending without no notice. */
	case BITTORRENT_STATE_OK:
		if (bittorrent->tracker.event == BITTORRENT_EVENT_STARTED) {
			assert(bittorrent->timer == TIMER_ID_UNDEF);
			bittorrent->tracker.started = 1;
			update_bittorrent_connection_state(conn);
		}

		set_bittorrent_tracker_interval(conn);
		bittorrent->tracker.event = BITTORRENT_EVENT_REGULAR;
		return;

	case BITTORRENT_STATE_OUT_OF_MEM:
		state = connection_state(S_OUT_OF_MEM);
		break;

	case BITTORRENT_STATE_REQUEST_FAILURE:
		add_bittorrent_message(conn->uri, connection_state(S_OK),
				       response);
		state = connection_state(S_OK);
		break;

	default:
		state = connection_state(S_BITTORRENT_TRACKER);
	}

	abort_connection(conn, state);
}

static void
check_bittorrent_stopped_request(void *____)
{
	struct uri *uri;
	int index;

	foreach_uri (uri, index, &bittorrent_stopped_requests) {
		init_bittorrent_fetch(NULL, uri, bittorrent_tracker_callback, NULL, 1);
	}

	free_uri_list(&bittorrent_stopped_requests);
}

/* Timer callback for @bittorrent->tracker.timer.  As explained in
 * @install_timer, this function must erase the expired timer ID from
 * all variables.  Also called from the request sending front-end. */
/* XXX: When called with event set to ``stopped'' failure handling should not
 * end the connection, since that is already in progress. */
/* TODO: Make a special timer callback entry point that can check if
 * rerequestion is needed, that is if more peer info is needed etc. */
static void
do_send_bittorrent_tracker_request(struct connection *conn)
{
	struct bittorrent_connection *bittorrent = conn->info;
	int stopped = (bittorrent->tracker.event == BITTORRENT_EVENT_STOPPED);
	unsigned char *ip, *key;
	struct string request;
	struct uri *uri = NULL;
	int numwant, index, min_size;

	bittorrent->tracker.timer = TIMER_ID_UNDEF;
	/* The expired timer ID has now been erased.  */

	/* If the previous request didn't make it, nuke it. This shouldn't
	 * happen but not doing this makes it a potential leak. */
	if (bittorrent->fetch)
		done_bittorrent_fetch(&bittorrent->fetch);

	if (!init_string(&request)) {
		done_string(&request);
		if (!stopped)
			abort_connection(conn,
					 connection_state(S_OUT_OF_MEM));
		return;
	}

	foreach_uri (uri, index, &bittorrent->meta.tracker_uris)
		/* Pick the first ...  */
		break;

	if (!uri) {
		done_string(&request);
		if (!stopped)
			abort_connection(conn,
					 connection_state(S_BITTORRENT_ERROR));
		return;
	}

	add_uri_to_string(&request, uri, URI_BASE);

	add_to_string(&request, "?info_hash=");
	encode_uri_string(&request, bittorrent->meta.info_hash,
			  sizeof(bittorrent->meta.info_hash), 1);

	add_to_string(&request, "&peer_id=");
	encode_uri_string(&request, bittorrent->peer_id,
			  sizeof(bittorrent->peer_id), 1);

	add_format_to_string(&request, "&uploaded=%ld", bittorrent->uploaded);
	add_format_to_string(&request, "&downloaded=%ld", bittorrent->downloaded);
	add_format_to_string(&request, "&left=%ld", bittorrent->left);

	/* Sending no IP-address is valid. The tracker figures it out
	 * automatically which is much easier. However, the user might want to
	 * configure a specific IP-address to send. */
	ip = get_opt_str("protocol.bittorrent.tracker.ip_address");
	if (*ip) add_format_to_string(&request, "&ip=%s", ip);

	/* This one is required for each request. */
	add_format_to_string(&request, "&port=%u", bittorrent->port);

	key = get_opt_str("protocol.bittorrent.tracker.key");
	if (*key) {
		add_to_string(&request, "&key=");
		encode_uri_string(&request, key, strlen(key), 1);
	}

	if (bittorrent->tracker.event != BITTORRENT_EVENT_REGULAR) {
		unsigned char *event;

		switch (bittorrent->tracker.event) {
		case BITTORRENT_EVENT_STARTED:
			event = "started";
			break;

		case BITTORRENT_EVENT_STOPPED:
			event = "stopped";
			break;

		case BITTORRENT_EVENT_COMPLETED:
			event = "completed";
			break;

		default:
			INTERNAL("Bad tracker event.");
			event = "";
		}

		add_format_to_string(&request, "&event=%s", event);
	}

	min_size = get_opt_int("protocol.bittorrent.tracker.min_skip_size");
	if (!min_size || list_size(&bittorrent->peer_pool) < min_size) {
		numwant = get_opt_int("protocol.bittorrent.tracker.numwant");
		/* Should the server default be used? */
		if (numwant == 0)
			numwant = -1;
	} else {
		numwant = -1;
	}

	if (numwant >= 0)
		add_format_to_string(&request, "&numwant=%d", numwant);

	if (get_opt_bool("protocol.bittorrent.tracker.compact"))
		add_to_string(&request, "&compact=1");

	uri = get_uri(request.source, 0);
	done_string(&request);
	if (!uri) {
		if (!stopped)
			abort_connection(conn,
					 connection_state(S_BITTORRENT_ERROR));
		return;
	}

	if (stopped) {
		/* We cannot start the event=stopped requesting directly here
		 * since we are nested inside a connection shutdown which means
		 * it will trigger a queue bug if we start adding a new
		 * connection. Solution: send the request in a bottom half. */
		if (register_bottom_half(check_bittorrent_stopped_request, NULL) == 0)
			add_to_uri_list(&bittorrent_stopped_requests, uri);

	} else {
		init_bittorrent_fetch(&bittorrent->fetch, uri,
				      bittorrent_tracker_callback, conn, 1);
	}

	done_uri(uri);
}


void
send_bittorrent_tracker_request(struct connection *conn)
{
	struct bittorrent_connection *bittorrent = conn->info;

	/* Kill the timer when we are not sending a periodic request to make
	 * sure that there are only one tracker request at any time. */
	kill_timer(&bittorrent->tracker.timer);

	do_send_bittorrent_tracker_request(conn);
}

void
done_bittorrent_tracker_connection(struct connection *conn)
{
	struct bittorrent_connection *bittorrent = conn->info;

	kill_timer(&bittorrent->tracker.timer);

	/* Nothing to shut down. */
	if (!bittorrent->tracker.started || bittorrent->tracker.failed)
		return;

	/* Send a tracker request with event=stopped. Note, the request won't be
	 * sent if we are shutting down due to an emergency, because the
	 * connection subsystem will be shut down soonish. */
	bittorrent->tracker.event = BITTORRENT_EVENT_STOPPED;
	send_bittorrent_tracker_request(conn);
}
