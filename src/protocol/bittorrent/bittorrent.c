/* Internal "bittorrent" protocol implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>

#include "elinks.h"

#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "network/state.h"
#include "protocol/bittorrent/bittorrent.h"
#include "protocol/bittorrent/common.h"
#include "protocol/bittorrent/dialogs.h"
#include "protocol/uri.h"
#include "session/session.h"


/* Hey, anything is possible, I am about to drink a carrot! */

static union option_info bittorrent_protocol_options[] = {
	INIT_OPT_TREE("protocol", N_("BitTorrent"),
		"bittorrent", 0,
		N_("BitTorrent specific options.")),

	/* ****************************************************************** */
	/* Listening socket options: */
	/* ****************************************************************** */

	INIT_OPT_TREE("protocol.bittorrent", N_("Port range"),
		"ports", 0,
		N_("Port range allowed to be used for listening on.")),

	INIT_OPT_INT("protocol.bittorrent.ports", N_("Minimum port"),
		"min", 0, LOWEST_PORT, HIGHEST_PORT, 6881,
		N_("The minimum port to try and listen on.")),

	INIT_OPT_INT("protocol.bittorrent.ports", N_("Maximum port"),
		"max", 0, LOWEST_PORT, HIGHEST_PORT, 6999,
		N_("The maximum port to try and listen on.")),


	/* ****************************************************************** */
	/* Tracker connection options: */
	/* ****************************************************************** */

	INIT_OPT_TREE("protocol.bittorrent", N_("Tracker"),
		"tracker", 0,
		N_("Tracker options.")),

	INIT_OPT_BOOL("protocol.bittorrent.tracker", N_("Use compact tracker format"),
		"compact", 0, 0,
		N_("Whether to request that the tracker returns peer info "
		"in compact format. Note, the compact format only supports "
		"IPv4 addresses.")),

	INIT_OPT_INT("protocol.bittorrent.tracker", N_("Tracker announce interval"),
		"interval", 0, 0, INT_MAX, 0,
		N_("The number of seconds to wait between periodically "
		"contacting the tracker for announcing progress and "
		"requesting more peers. Set to zero to use the interval "
		"requested by the tracker.")),

	INIT_OPT_STRING("protocol.bittorrent.tracker", N_("IP-address to announce"),
		"ip_address", 0, "",
		N_("What IP address to report to the tracker. If set to \"\" "
		"no IP address will be sent and the tracker will "
		"automatically determine an appropriate IP address.")),

	INIT_OPT_STRING("protocol.bittorrent.tracker", N_("User identification string"),
		"key", 0, "",
		N_("An additional identification that is not shared with any "
		"users. It is intended to allow a client to prove their "
		"identity should their IP address change. It is an optional "
		"parameter, but some trackers require this parameter. "
		"If set to \"\" no user key will be sent to the tracker.")),

	INIT_OPT_INT("protocol.bittorrent.tracker", N_("Maximum number of peers to request"),
		"numwant", 0, 0, INT_MAX, 50,
		N_("The maximum number of peers to request from the tracker. "
		"Set to 0 to use the server default.")),

	INIT_OPT_INT("protocol.bittorrent.tracker", N_("Minimum peers to skip rerequesting"),
		"min_skip_size", 0, 0, INT_MAX, 20,
		N_("The minimum number of peers to have in the current peer "
		"info pool before skipping requesting of more peers. I.e. "
		"setting numwant to zero. Set to 0 to not have any limit.")),


	/* ****************************************************************** */
	/* Lowlevel peer-wire options: */
	/* ****************************************************************** */

	INIT_OPT_TREE("protocol.bittorrent", N_("Peer-wire"),
		"peerwire", 0,
		N_("Lowlevel peer-wire options.")),

	INIT_OPT_INT("protocol.bittorrent.peerwire", N_("Maximum number of peer connections"),
		"connections", 0, 1, INT_MAX, 55,
		N_("The maximum number of allowed connections to both active "
		"and non-active peers. By increasing the number of allowed "
		"connections, the chance of finding good peers to download "
		"from is increased. However, too many connections can lead to "
		"TCP congestion. If the maximum is reached all new incoming "
		"connections will be closed.")),

	INIT_OPT_INT("protocol.bittorrent.peerwire", N_("Maximum peer message length"),
		"max_message_length", 0, 1, INT_MAX, BITTORRENT_MESSAGE_MAX_SIZE,
		N_("The maximum length of messages to accept over the wire. "
		"Larger values will cause the connection to be dropped.")),

	INIT_OPT_INT("protocol.bittorrent.peerwire", N_("Maximum allowed request length"),
		"max_request_length", 0, 1, INT_MAX, BITTORRENT_REQUEST_ACCEPT_LENGTH,
		N_("The maximum length to allow for incoming requests. "
		"Larger requests will cause the connection to be dropped.")),

	INIT_OPT_INT("protocol.bittorrent.peerwire", N_("Length of requests"),
		"request_length", 0, 1, INT_MAX, BITTORRENT_REQUEST_LENGTH,
		N_("How many bytes to query for per request. This is "
		"complementary to the max_request_length option. "
		"If the configured length is bigger than the piece length "
		"it will be truncated.")),

	INIT_OPT_INT("protocol.bittorrent.peerwire", N_("Peer inactivity timeout"),
		"timeout", 0, 0, INT_MAX, 300,
		N_("The number of seconds to wait before closing a socket on "
		"which nothing has been received or sent.")),

	INIT_OPT_INT("protocol.bittorrent.peerwire", N_("Maximum peer pool size"),
		"pool_size", 0, 0, INT_MAX, 55,
		N_("Maximum number of items in the peer pool. The peer pool "
		"contains information used for establishing connections to "
		"new peers.\n"
		"\n"
		"Set to 0 to have unlimited size.")),


	/* ****************************************************************** */
	/* Piece management options: */
	/* ****************************************************************** */

	INIT_OPT_INT("protocol.bittorrent", N_("Maximum piece cache size"),
		"piece_cache_size", 0, 0, INT_MAX, 1024 * 1024,
		N_("The maximum amount of memory used to hold recently "
		"downloaded pieces.\n"
		"\n"
		"Set to 0 to have unlimited size.")),

	/* ****************************************************************** */
	/* Strategy options: */
	/* ****************************************************************** */

#if 0
	INIT_OPT_STRING("protocol.bittorrent", N_("Sharing rate"),
		"sharing_rate", 0, "1.0",
		N_("The minimum sharing rate to achieve before stop seeding. "
		"The sharing rate is computed as the number of uploaded bytes "
		"divided with the number of downloaded bytes. The value "
		"should be a double value between 0.0 and 1.0 both included. "
		"Set to 1.0 to atleast upload a complete copy of all data and "
		"set to 0.0 to have unlimited sharing rate.")),
#endif
	INIT_OPT_INT("protocol.bittorrent", N_("Maximum number of uploads"),
		"max_uploads", 0, 0, INT_MAX, 7,
		N_("The maximum number of uploads to allow at once.")),

	/* The number of uploads to fill out to with extra optimistic unchokes */
	INIT_OPT_INT("protocol.bittorrent", N_("Minimum number of uploads"),
		"min_uploads", 0, 0, INT_MAX, 2,
		N_("The minimum number of uploads which should at least "
		"be used for new connections.")),

#if 0
	INIT_OPT_INT("protocol.bittorrent", N_("Keepalive interval"),
		"keepalive_interval", 0, 0, INT_MAX, 120,
		N_("The number of seconds to pause between sending keepalive "
		"messages.")),
#endif
	INIT_OPT_INT("protocol.bittorrent", N_("Number of pending requests"),
		"request_queue_size", 0, 1, INT_MAX, 5,
		N_("How many piece requests to continuously keep in queue. "
		"Pipelining of requests is essential to saturate connections "
		"and get a good connection performance and thus a faster "
		"download. However, a very big queue size can lead to wasting "
		"bandwidth near the end of the connection since remaining "
		"piece blocks will be requested from multiple peers.")),

#if 0
	/* Bram uses 30 seconds here. */
	INIT_OPT_INT("protocol.bittorrent", N_("Peer snubbing interval"),
		"snubbing_interval", 0, 0, INT_MAX, 30,
		N_("The number of seconds to wait for file data before "
		"assuming the peer has been snubbed.")),
#endif
	INIT_OPT_INT("protocol.bittorrent", N_("Peer choke interval"),
		"choke_interval", 0, 0, INT_MAX, BITTORRENT_DEFAULT_CHOKE_INTERVAL,
		N_("The number of seconds between updating the connection "
		"state and most importantly choke and unchoke peer "
		"connections. The choke period should be big enough for newly "
		"unchoked connections to get started but small enough to not "
		"allow freeriders too much room for stealing bandwidth.")),

	INIT_OPT_INT("protocol.bittorrent", N_("Rarest first piece selection cutoff"),
		"rarest_first_cutoff", 0, 0, INT_MAX, 4,
		N_("The number of pieces to obtain before switching piece "
		"selection strategy from random to rarest first.")),

	INIT_OPT_BOOL("protocol.bittorrent", N_("Allow blacklisting"),
		"allow_blacklist", 0, 1,
		N_("Allow blacklisting of buggy peers.")),

	NULL_OPTION_INFO,
};

uint32_t
get_bittorrent_peerwire_max_message_length(void)
{
	return get_opt_int_tree(&bittorrent_protocol_options[0].option,
				"peerwire.max_message_length");
}

uint32_t
get_bittorrent_peerwire_max_request_length(void)
{
	return get_opt_int_tree(&bittorrent_protocol_options[0].option,
				"peerwire.max_request_length");
}


/* File selection store. */
static INIT_LIST_OF(struct bittorrent_selection_info, bittorrent_selections);

struct bittorrent_selection_info {
	LIST_HEAD(struct bittorrent_selection_info);

	/* Identifier used for saving and getting meta info from the meta info
	 * store. */
	struct uri *uri;

	size_t size;
	int *selection;
};

int *
get_bittorrent_selection(struct uri *uri, size_t size)
{
	struct bittorrent_selection_info *info;

	foreach (info, bittorrent_selections) {
		if (compare_uri(info->uri, uri, 0)) {
			int *selection;

			del_from_list(info);

			if (info->size == size) {
				selection = info->selection;

			} else {
				mem_free(info->selection);
				selection = NULL;
			}

			done_uri(info->uri);
			mem_free(info);

			return selection;
		}
	}

	return NULL;
}

void
add_bittorrent_selection(struct uri *uri, int *selection, size_t size)
{
	struct bittorrent_selection_info *info;
	int *duplicate;

	duplicate = get_bittorrent_selection(uri, size);
	if (duplicate) mem_free(duplicate);

	info = mem_calloc(1, sizeof(*info));
	if (!info) return;

	info->uri = get_uri_reference(uri);
	info->size = size;
	info->selection = selection;

	add_to_list(bittorrent_selections, info);
}


/* Message queue. */
static INIT_LIST_OF(struct bittorrent_message, bittorrent_messages);

void
add_bittorrent_message(struct uri *uri, struct connection_state state,
		       struct string *string)
{
	struct bittorrent_message *message;
	int length = string ? string->length : 0;

	message = mem_calloc(1, sizeof(*message) + length);
	if (!message) return;

	message->uri = get_uri_reference(uri);
	message->state = state;
	if (length)
		memcpy(message->string, string->source, string->length);
	add_to_list(bittorrent_messages, message);

	add_questions_entry(bittorrent_message_dialog, message);
}

static void
done_bittorrent(struct module *module)
{
	struct bittorrent_message *message, *message_next;
	struct bittorrent_selection_info *info, *info_next;

	done_bittorrent_blacklist();

	foreachsafe (message, message_next, bittorrent_messages)
		done_bittorrent_message(message);

	foreachsafe (info, info_next, bittorrent_selections) {
		del_from_list(info);
		mem_free(info->selection);
		mem_free(info);
	}
}


struct module bittorrent_protocol_module = struct_module(
	/* name: */		N_("BitTorrent"),
	/* options: */		bittorrent_protocol_options,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		done_bittorrent
);
