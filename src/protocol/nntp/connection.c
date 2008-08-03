/* Connection and data transport handling */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "network/connection.h"
#include "network/socket.h"
#include "protocol/nntp/codes.h"
#include "protocol/nntp/connection.h"
#include "protocol/nntp/nntp.h"
#include "protocol/nntp/response.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "util/memory.h"
#include "util/string.h"


/* The official color for this planet is green,
 * which grows in pockets of them people willing to scheme. --delasoul */

static void nntp_send_command(struct connection *conn);


/* Connection setup: */

/* Resolves the target by looking at the part of the URI _after_ last '/' */
static enum nntp_target
get_nntp_target(unsigned char *data, int datalen)
{
	enum nntp_target target = NNTP_TARGET_ARTICLE_NUMBER;
	int pos;

	/* List groups on the server if there is nothing specified */
	if (!datalen)
		return NNTP_TARGET_GROUPS;

	/* Check for <message-id> */
	if (memchr(data, '@', datalen))
		return NNTP_TARGET_MESSAGE_ID;

	/* Check for <article-number> or <start-number>-<end-number> */
	for (pos = 0; pos < datalen; pos++) {
		if (!isdigit(data[pos])) {
			/* Assume <group> if it is not a range dash
			 * or a range dash was already detected */
			if (data[pos] != '-'
			    || target == NNTP_TARGET_ARTICLE_RANGE)
				return NNTP_TARGET_GROUP;

			target = NNTP_TARGET_ARTICLE_RANGE;
		}
	}

	/* If the scanning saw only numberical char and possibly one '-' it
	 * defaults to assume the URI part describes article number or range.
	 * This rules out numerical only group names. */
	return target;
}

/* Init article range and check that both <start-number> and <end-number> are
 * non empty and valid numbers and that the range is valid. */
static int
init_nntp_article_range(struct nntp_connection_info *nntp,
			unsigned char *data, int datalen)
{
	long start_number, end_number;
	unsigned char *end;

	errno = 0;
	start_number = strtol(data, (char **) &end, 10);
	if (errno || !end || *end != '-' || start_number < 0)
		return 0;

	end_number = strtol(end + 1, (char **) &end, 10);
	if (errno || end != data + datalen || end_number < 0)
		return 0;

	nntp->current_article = start_number;
	nntp->end_article     = end_number;

	nntp->message.source  = data;
	nntp->message.length  = datalen;

	return start_number <= end_number ? 1 : 0;
}

static struct nntp_connection_info *
init_nntp_connection_info(struct connection *conn)
{
	struct uri *uri = conn->uri;
	struct nntp_connection_info *nntp;
	unsigned char *groupend;
	unsigned char *data;
	int datalen;

	nntp = mem_calloc(1, sizeof(*nntp));
	if (!nntp) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return NULL;
	}

	conn->info = nntp;

	set_string_magic(&nntp->group);
	set_string_magic(&nntp->message);

	data    = uri->data;
	datalen = uri->datalen;

	/* Check for <group>/ */
	groupend = memchr(data, '/', datalen);
	if (groupend) {
		int grouplen = groupend - data;

		nntp->group.source = data;
		nntp->group.length = grouplen;

		datalen -= grouplen + 1;
		data    += grouplen + 1;

		/* Nothing after last '/'? */
		if (!datalen) {
			nntp->target = NNTP_TARGET_GROUP;
			return nntp;
		}
	}

	nntp->target = get_nntp_target(data, datalen);

	switch (nntp->target) {
	case NNTP_TARGET_ARTICLE_RANGE:
		if (init_nntp_article_range(nntp, data, datalen))
			break;

		/* FIXME: Special S_NNTP_BAD_RANGE */
		abort_connection(conn, connection_state(S_BAD_URL));
		return NULL;

	case NNTP_TARGET_ARTICLE_NUMBER:
	case NNTP_TARGET_MESSAGE_ID:
	case NNTP_TARGET_GROUP_MESSAGE_ID:
		nntp->message.source = data;
		nntp->message.length = datalen;
		break;

	case NNTP_TARGET_GROUP:
		/* Map nntp://<server>/<group> to nntp://<server>/<group>/ so
		 * we get only one cache entry with content. */
		if (!groupend) {
			struct connection_state state = connection_state(S_OK);

			conn->cached = get_cache_entry(conn->uri);
			if (!conn->cached
			    || !redirect_cache(conn->cached, "/", 0, 0))
				state = connection_state(S_OUT_OF_MEM);

			abort_connection(conn, state);
			return NULL;
		}

		/* Reject nntp://<server>/<group>/<group> */
		if (nntp->group.source)	{
			abort_connection(conn, connection_state(S_BAD_URL));
			return NULL;
		}

		nntp->group.source = data;
		nntp->group.length = datalen;
		break;

	case NNTP_TARGET_GROUPS:
	case NNTP_TARGET_QUIT:
		break;
	}

	return nntp;
}


/* Connection teardown: */

static void
nntp_quit(struct connection *conn)
{
	struct nntp_connection_info *info;

	info = mem_calloc(1, sizeof(*info));
	if (!info) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	info->target = NNTP_TARGET_QUIT;

	conn->info = info;

	nntp_send_command(conn);
}

static void
nntp_end_request(struct connection *conn, struct connection_state state)
{
	struct nntp_connection_info *nntp = conn->info;

	if (nntp->target == NNTP_TARGET_QUIT) {
		abort_connection(conn, state);
		return;
	}

	if (is_in_state(state, S_OK)) {
		if (conn->cached) {
			normalize_cache_entry(conn->cached, conn->from);
		}
	} else if (is_in_state(state, S_OUT_OF_MEM)) {
		/* FIXME: Clear the socket buffers before ending so the one
		 * grabing the keepalive connection will be able to go on. */
	}

	set_connection_state(conn, state);
	add_keepalive_connection(conn, NNTP_KEEPALIVE_TIMEOUT, nntp_quit);
}


/* Reponse receiving: */

static void
read_nntp_data(struct socket *socket, struct read_buffer *rb)
{
	struct connection *conn = socket->conn;

	if (socket->state == SOCKET_CLOSED) {
		nntp_end_request(conn, connection_state(S_OK));
		return;
	}

	switch (read_nntp_response_data(conn, rb).basic) {
	case S_OK:
		nntp_send_command(conn);
		break;

	case S_OUT_OF_MEM:
		nntp_end_request(conn, connection_state(S_OUT_OF_MEM));
		break;

	case S_TRANS:
	default:
		read_from_socket(conn->socket, rb, connection_state(S_TRANS),
				 read_nntp_data);
	}
}

/* Translate NNTP code to the internal connection state. */
static struct connection_state
get_nntp_connection_state(enum nntp_code code)
{
	switch (code) {
	case NNTP_CODE_400_GOODBYE:		return connection_state(S_NNTP_SERVER_HANG_UP);
	case NNTP_CODE_411_GROUP_UNKNOWN:	return connection_state(S_NNTP_GROUP_UNKNOWN);
	case NNTP_CODE_423_ARTICLE_NONUMBER:	return connection_state(S_NNTP_ARTICLE_UNKNOWN);
	case NNTP_CODE_430_ARTICLE_NOID:	return connection_state(S_NNTP_ARTICLE_UNKNOWN);
	case NNTP_CODE_436_ARTICLE_TRANSFER:	return connection_state(S_NNTP_TRANSFER_ERROR);
	case NNTP_CODE_480_AUTH_REQUIRED:	return connection_state(S_NNTP_AUTH_REQUIRED);
	case NNTP_CODE_502_ACCESS_DENIED:	return connection_state(S_NNTP_ACCESS_DENIED);
	case NNTP_CODE_503_PROGRAM_FAULT:	return connection_state(S_NNTP_SERVER_ERROR);

	case NNTP_CODE_412_GROUP_UNSET:
	case NNTP_CODE_420_ARTICLE_UNSET:
	case NNTP_CODE_421_ARTICLE_NONEXT:
	case NNTP_CODE_422_ARTICLE_NOPREV:
	case NNTP_CODE_435_ARTICLE_NOSEND:
	case NNTP_CODE_437_ARTICLE_REJECTED:
	case NNTP_CODE_440_POSTING_DENIED:
	case NNTP_CODE_441_POSTING_FAILED:
	case NNTP_CODE_482_AUTH_REJECTED:
	case NNTP_CODE_580_AUTH_FAILED:
	case NNTP_CODE_500_COMMAND_UNKNOWN:
	case NNTP_CODE_501_COMMAND_SYNTAX:
	default:
		/* Notice and error codes for stuff which is either not
		 * supported or which is not supposed to happen. */
		return connection_state(S_NNTP_ERROR);
	};
}

static void
nntp_got_response(struct socket *socket, struct read_buffer *rb)
{
	struct connection *conn = socket->conn;
	struct nntp_connection_info *nntp = conn->info;

	if (socket->state == SOCKET_CLOSED) {
		nntp_end_request(conn, connection_state(S_OK));
		return;
	}

	nntp->code = get_nntp_response_code(conn, rb);

	switch (nntp->code) {
	case NNTP_CODE_NONE:
		read_from_socket(conn->socket, rb, connection_state(S_TRANS),
				 nntp_got_response);
		break;

	case NNTP_CODE_INVALID:
		nntp_end_request(conn, connection_state(S_NNTP_ERROR));
		break;

	case NNTP_CODE_200_HELLO:
	case NNTP_CODE_201_HELLO_NOPOST:
	case NNTP_CODE_211_GROUP_SELECTED:
		nntp_send_command(conn);
		break;

	case NNTP_CODE_205_GOODBYE:
		nntp_end_request(conn, connection_state(S_OK));
		break;

	case NNTP_CODE_215_FOLLOW_GROUPS:
	case NNTP_CODE_220_FOLLOW_ARTICLE:
	case NNTP_CODE_221_FOLLOW_HEAD:
	case NNTP_CODE_222_FOLLOW_BODY:
	case NNTP_CODE_223_FOLLOW_NOTEXT:
	case NNTP_CODE_224_FOLLOW_XOVER:
	case NNTP_CODE_230_FOLLOW_NEWNEWS:
	case NNTP_CODE_231_FOLLOW_NEWGROUPS:
		read_nntp_data(socket, rb);
		break;

	case NNTP_CODE_500_COMMAND_UNKNOWN:
		if (nntp->command == NNTP_COMMAND_LIST_ARTICLES
		    && !nntp->xover_unsupported) {
			nntp->xover_unsupported = 1;
			nntp_send_command(conn);
			break;
		}

	default:
		/* FIXME: Handle (error) response codes */
		nntp_end_request(conn, get_nntp_connection_state(nntp->code));
		break;
	}
}

static void
nntp_get_response(struct socket *socket)
{
	struct connection *conn = socket->conn;
	struct read_buffer *rb = alloc_read_buffer(conn->socket);

	if (!rb) return;

	socket->state = SOCKET_END_ONCLOSE;
	read_from_socket(conn->socket, rb, conn->state, nntp_got_response);
}


/* Command sending: */

/* Figure out the next command by looking at the target and the previous
 * command. */
static enum nntp_command
get_nntp_command(struct nntp_connection_info *nntp)
{
	/* The more logical approach of making the switch noodle use the target
	 * actually will make it bigger altho' assertions of invalid states
	 * would be easier. I chose to make it simpler for now. --jonas */
	/* All valid states should return from within the outer switch. Anything
	 * else should break out to the function end and cause an internal
	 * error message. */
	switch (nntp->command) {
	case NNTP_COMMAND_NONE:
		switch (nntp->target) {
		case NNTP_TARGET_GROUPS:
			return NNTP_COMMAND_LIST_NEWSGROUPS;

		case NNTP_TARGET_GROUP:
		case NNTP_TARGET_ARTICLE_NUMBER:
		case NNTP_TARGET_ARTICLE_RANGE:
		case NNTP_TARGET_GROUP_MESSAGE_ID:
			return NNTP_COMMAND_GROUP;

		case NNTP_TARGET_MESSAGE_ID:
			return NNTP_COMMAND_ARTICLE_MESSAGE_ID;

		case NNTP_TARGET_QUIT:
			return NNTP_COMMAND_QUIT;
		}
		break;

	case NNTP_COMMAND_GROUP:
		switch (nntp->target) {
		case NNTP_TARGET_GROUP:
		case NNTP_TARGET_ARTICLE_RANGE:
			return NNTP_COMMAND_LIST_ARTICLES;

		case NNTP_TARGET_ARTICLE_NUMBER:
			return NNTP_COMMAND_ARTICLE_NUMBER;

		case NNTP_TARGET_GROUP_MESSAGE_ID:
			return NNTP_COMMAND_ARTICLE_MESSAGE_ID;

		case NNTP_TARGET_MESSAGE_ID:
		case NNTP_TARGET_QUIT:
		case NNTP_TARGET_GROUPS:
			break;
		}
		break;

	case NNTP_COMMAND_LIST_ARTICLES:
		if (nntp->target != NNTP_TARGET_GROUP
		    && nntp->target != NNTP_TARGET_ARTICLE_RANGE)
			break;

		/* Are there more articles to list? */
		if (nntp->xover_unsupported
		    && nntp->current_article <= nntp->end_article)
			return NNTP_COMMAND_LIST_ARTICLES;

		return NNTP_COMMAND_NONE;

	case NNTP_COMMAND_ARTICLE_NUMBER:
		if (nntp->target == NNTP_TARGET_ARTICLE_NUMBER)
			return NNTP_COMMAND_NONE;

		return NNTP_COMMAND_NONE;

	case NNTP_COMMAND_ARTICLE_MESSAGE_ID:
	case NNTP_COMMAND_LIST_NEWSGROUPS:
	case NNTP_COMMAND_QUIT:
		/* FIXME: assert()? --jonas */
		return NNTP_COMMAND_NONE;
	}

	INTERNAL("Bad command %d handling %d", nntp->target, nntp->command);

	return NNTP_COMMAND_NONE;
}

/* Command lines shall not exceed 512 characters in length, counting all
 * characters including spaces, separators, punctuation, and the trailing CR-LF
 * (Carriage Return - Line Feed) pair. */
#define NNTP_MAX_COMMAND_LENGTH	512

/* FIXME: For the beauty maybe %.*s could be used. --jonas */
static void
add_nntp_command_to_string(struct string *req, struct nntp_connection_info *nntp)
{
	switch (nntp->command) {
	case NNTP_COMMAND_GROUP:
		add_to_string(req, "GROUP ");
		add_string_to_string(req, &nntp->group);
		break;

	case NNTP_COMMAND_ARTICLE_NUMBER:
		add_to_string(req, "ARTICLE ");
		add_string_to_string(req, &nntp->message);
		break;

	case NNTP_COMMAND_ARTICLE_MESSAGE_ID:
		add_to_string(req, "ARTICLE <");
		add_string_to_string(req, &nntp->message);
		add_char_to_string(req, '>');
		break;

	case NNTP_COMMAND_LIST_ARTICLES:
		if (!nntp->xover_unsupported) {
			int first = nntp->current_article++;
			int last = nntp->end_article;

			add_format_to_string(req, "XOVER %d-%d", first, last);
			break;
		}

		assert(nntp->current_article <= nntp->end_article);

		add_format_to_string(req, "HEAD %ld", nntp->current_article++);
		break;

	case NNTP_COMMAND_QUIT:
		add_to_string(req, "QUIT");
		break;

	case NNTP_COMMAND_LIST_NEWSGROUPS:
		add_to_string(req, "LIST NEWSGROUPS");
		break;

	case NNTP_COMMAND_NONE:
		INTERNAL("Trying to add 'no' command.");
		return;
	}

	add_crlf_to_string(req);
}

static void
nntp_send_command(struct connection *conn)
{
	struct nntp_connection_info *nntp = conn->info;
	struct string req;

	nntp->command = get_nntp_command(nntp);

	if (nntp->command == NNTP_COMMAND_NONE) {
		nntp_end_request(conn, connection_state(S_OK));
		return;
	}

	if (!init_string(&req)) {
		nntp_end_request(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	/* FIXME: Check non empty and < NNTP_MAX_COMMAND_LENGTH */
	add_nntp_command_to_string(&req, nntp);

	request_from_socket(conn->socket, req.source, req.length,
			    connection_state(S_SENT),
			    SOCKET_END_ONCLOSE, nntp_got_response);
	done_string(&req);
}


/* The ``nntp://'' and ``news:'' protocol handlers: */

void
nntp_protocol_handler(struct connection *conn)
{
	if (!init_nntp_connection_info(conn))
		return;

	if (!has_keepalive_connection(conn)) {
		make_connection(conn->socket, conn->uri, nntp_get_response,
				conn->cache_mode >= CACHE_MODE_FORCE_RELOAD);
	} else {
		nntp_send_command(conn);
	}
}

/* Note that while nntp:// URIs specify a unique location for the article
 * resource, most NNTP servers currently on the Internet today are configured
 * only to allow access from local clients, and thus nntp:// URIs do not
 * designate globally accessible resources. Thus, the news: form of URI is
 * preferred as a way of identifying news articles.
 *
 * Whenever dealing with news: URIs the <server> part is retrieved from either
 * protocol.nntp.server or the NNTPSERVER environment variable. The news: URI
 * is then redirected to a nntp:// URI using the <server> so that:
 *
 *	news:<message-id>	-> nntp://<server>/<message-id>
 *	news:<group>		-> nntp://<server>/<group>
 *	news:<group>/<...>	-> nntp://<server>/<group>/<...>
 */

void
news_protocol_handler(struct connection *conn)
{
	unsigned char *protocol;
	unsigned char *server = get_nntp_server();
	struct string location;

	if (!*server) server = getenv("NNTPSERVER");
	if (!server || !*server) {
		abort_connection(conn, connection_state(S_NNTP_NEWS_SERVER));
		return;
	}

	conn->cached = get_cache_entry(conn->uri);
	if (!conn->cached || !init_string(&location)) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	protocol = conn->uri->protocol == PROTOCOL_NEWS ? "nntp" : "nntps";

	add_format_to_string(&location, "%s://%s/", protocol, server);
	add_uri_to_string(&location, conn->uri, URI_DATA);

	redirect_cache(conn->cached, location.source, 0, 0);

	done_string(&location);

	abort_connection(conn, connection_state(S_OK));
}
