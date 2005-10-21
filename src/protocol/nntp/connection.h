
#ifndef EL__PROTOCOL_NNTP_CONNECTION_H
#define EL__PROTOCOL_NNTP_CONNECTION_H

#include "protocol/nntp/codes.h"
#include "protocol/protocol.h"
#include "util/string.h"

/* An NNTP target is a mapping from a given URI which in short form tells what
 * the URI describes. Using the following tokens:
 *
 *	<group>			the name of the newsgroup
 *	<message-id>		<unique>@<full_domain_name>
 *	<article-number>	a numeric id of the article within the group
 *	<article-range>		<start-number>-<end-number>
 *
 * The nntp:// URI syntax is resolved like this:
 *
 *	nntp://<server>/			 - NNTP_TARGET_GROUPS
 *	nntp://<server>/<message-id>		 - NNTP_TARGET_MESSAGE_ID
 *	nntp://<server>/<group>			 - NNTP_TARGET_GROUP
 *	nntp://<server>/<group>/		 - NNTP_TARGET_GROUP
 *	nntp://<server>/<group>/<article-number> - NNTP_TARGET_ARTICLE_NUMBER
 *	nntp://<server>/<group>/<article-range>  - NNTP_TARGET_ARTICLE_RANGE
 *	nntp://<server>/<group>/<message-id>	 - NNTP_TARGET_GROUP_MESSAGE_ID
 */

/* NNTP targets */
enum nntp_target {
	NNTP_TARGET_GROUPS,		/* List groups available */
	NNTP_TARGET_MESSAGE_ID,		/* Get <message-id> */
	NNTP_TARGET_GROUP,		/* List messages in <group> */
	NNTP_TARGET_ARTICLE_NUMBER,	/* Get <article-number> from <group> */
	NNTP_TARGET_ARTICLE_RANGE,	/* Get <article-range> from <group> */
	NNTP_TARGET_GROUP_MESSAGE_ID,	/* Get <message-id> from <group> */
	NNTP_TARGET_QUIT,		/* Special target for shutting down */
};

/* NNTP command identifiers */
enum nntp_command {
	NNTP_COMMAND_NONE,		/* No command currently in progress */
	NNTP_COMMAND_QUIT,		/* QUIT */
	NNTP_COMMAND_GROUP,		/* GROUP <group> */
	NNTP_COMMAND_ARTICLE_NUMBER,	/* ARTICLE <article-number> */
	NNTP_COMMAND_ARTICLE_MESSAGE_ID,/* ARTICLE <message-id> */
	NNTP_COMMAND_LIST_NEWSGROUPS,	/* LIST NEWSGROUP */
	NNTP_COMMAND_LIST_ARTICLES,	/* XOVER or HEAD for each article */
};

/* This stores info about an active NNTP connection */
struct nntp_connection_info {
	/* The target denotes what is the purpose of the connection. What
	 * should be requested from the server. */
	enum nntp_target target;

	/* There's quite a few callbacks involved in requesting the target so
	 * to figure out whazzup the ``id'' of the current running command is
	 * saved. */
	enum nntp_command command;

	/* The current NNTP status or response code received from the server */
	enum nntp_code code;

	/* Strings pointing into the connection URI. They caches info useful
	 * for requesting the target. */

	/* The <group> or undefined if target is NNTP_TARGET_GROUPS */
	struct string group;

	/* Can contain either <message-id>, <article-number> or <article-range>
	 * or undefined if target is NTTP_TARGET_{GROUP,GROUPS}. */
	/* For <article-range> it contains start and end numbers as well as the
	 * separating '-'. */
	struct string message;

	/* State for getting articles in a <group> or an <article-range> */
	long current_article, end_article, articles;

	/* When listing articles in a <group> using the XOVER command is
	 * preferred however not all news servers support XOVER so ... */
	unsigned int xover_unsupported:1;
};

#ifdef CONFIG_NNTP
extern protocol_handler_T nntp_protocol_handler;
extern protocol_handler_T news_protocol_handler;
#else
#define nntp_protocol_handler NULL
#define news_protocol_handler NULL
#endif

#endif
