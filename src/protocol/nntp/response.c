/* Parses and converts NNTP responses to enum values and cache entry HTML */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "intl/gettext/libintl.h"
#include "mime/backend/common.h"
#include "network/connection.h"
#include "network/socket.h"
#include "protocol/header.h"
#include "protocol/nntp/codes.h"
#include "protocol/nntp/connection.h"
#include "protocol/nntp/nntp.h"
#include "protocol/nntp/response.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


/* Search for line ending \r\n pair */
static unsigned char *
get_nntp_line_end(unsigned char *data, int datalen)
{
	for (; datalen > 1; data++, datalen--)
		if (data[0] == ASCII_CR && data[1] == ASCII_LF)
			return data + 2;

	return NULL;
}

/* RFC 977 - Section 2.4.1.  Text Responses:
 *
 * A single line containing only a period (.) is sent to indicate the end of
 * the text (i.e., the server will send a CR-LF pair at the end of the last
 * line of text, a period, and another CR-LF pair).
 *
 * If the text contained a period as the first character of the text line in
 * the original, that first period is doubled.  Therefore, the client must
 * examine the first character of each line received, and for those beginning
 * with a period, determine either that this is the end of the text or whether
 * to collapse the doubled period to a single one. */
/* Returns NULL if end-of-text is found else start of collapsed line */
static inline unsigned char *
check_nntp_line(unsigned char *line, unsigned char *end)
{
	assert(line < end);

	/* Just to be safe NUL terminate the line */
	end[-2] = 0;

	if (line[0] != '.') return line;

	if (!line[1]) return NULL;
	if (line[1] == '.') line++;

	return line;
}

static inline unsigned char *
get_nntp_message_header_end(unsigned char *data, int datalen)
{
	unsigned char *end, *prev_end = data;

	while ((end = get_nntp_line_end(data, datalen))) {
		datalen	-= end - data;
		data	 = end;

		/* If only \r\n is there */
		if (prev_end + 2 == end) {
			/* NUL terminate the header so that it ends with just
			 * one \r\n usefull for appending to cached->head. */
			end[-2] = 0;
			return end;
		}

		prev_end = end;
	}

	return NULL;
}

static enum connection_state
init_nntp_header(struct connection *conn, struct read_buffer *rb)
{
	struct nntp_connection_info *nntp = conn->info;

	if (!conn->cached) {
		conn->cached = get_cache_entry(conn->uri);
		if (!conn->cached) return S_OUT_OF_MEM;

	} else if (conn->cached->head || conn->cached->content_type) {
		/* If the head is set wipe out the content to be sure */
		delete_entry_content(conn->cached);
		mem_free_set(&conn->cached->head, NULL);
	}

	/* XXX: Override any Content-Type line in the header */
	mem_free_set(&conn->cached->content_type, stracpy("text/html"));
	if (!conn->cached->content_type)
		return S_OUT_OF_MEM;

	switch (nntp->target) {
	case NNTP_TARGET_ARTICLE_NUMBER:
	case NNTP_TARGET_MESSAGE_ID:
	case NNTP_TARGET_GROUP_MESSAGE_ID:
	{
		unsigned char *end;

		end = get_nntp_message_header_end(rb->data, rb->length);
		if (!end) {
			/* Redo the whole cache entry thing next time */
			return S_TRANS;
		}

		/* FIXME: Add the NNTP response code line */
		conn->cached->head = stracpy("FIXME NNTP response code\r\n");
		if (!conn->cached->head) return S_OUT_OF_MEM;

		add_to_strn(&conn->cached->head, rb->data);

		/* ... and remove it */
		conn->received += end - rb->data;
		kill_buffer_data(rb, end - rb->data);
		break;
	}
	case NNTP_TARGET_ARTICLE_RANGE:
	case NNTP_TARGET_GROUP:
	case NNTP_TARGET_GROUPS:
	case NNTP_TARGET_QUIT:
		break;
	}

	return S_OK;
}


static unsigned char *
get_nntp_title(struct connection *conn)
{
	struct nntp_connection_info *nntp = conn->info;
	struct string title;

	if (!init_string(&title))
		return NULL;

	switch (nntp->target) {
	case NNTP_TARGET_ARTICLE_RANGE:
		add_format_to_string(&title, "Articles in the range %ld to %ld",
				     nntp->current_article, nntp->end_article);
		break;

	case NNTP_TARGET_ARTICLE_NUMBER:
	case NNTP_TARGET_MESSAGE_ID:
	case NNTP_TARGET_GROUP_MESSAGE_ID:
	{
		unsigned char *subject;

		subject = parse_header(conn->cached->head, "Subject", NULL);
		if (subject) {
			add_to_string(&title, subject);
			mem_free(subject);
			break;
		}

		add_format_to_string(&title, "Article "),
		add_string_to_string(&title, &nntp->message);

		if (nntp->target == NNTP_TARGET_MESSAGE_ID)
			break;

		add_format_to_string(&title, " in ");
		add_string_to_string(&title, &nntp->group);
		break;
	}
	case NNTP_TARGET_GROUP:
		add_format_to_string(&title, "Articles in "),
		add_string_to_string(&title, &nntp->group);
		break;

	case NNTP_TARGET_GROUPS:
		add_format_to_string(&title, "Newsgroups on "),
		add_uri_to_string(&title, conn->uri, URI_PUBLIC);
		break;

	case NNTP_TARGET_QUIT:
		break;
	}

	return title.source;
}

static void
add_nntp_html_start(struct string *html, struct connection *conn)
{
	struct nntp_connection_info *nntp = conn->info;
	unsigned char *title = get_nntp_title(conn);

	add_format_to_string(html,
		"<html>\n"
		"<head><title>%s</title></head>\n"
		"<body>\n",
		empty_string_or_(title));

	switch (nntp->target) {
	case NNTP_TARGET_ARTICLE_NUMBER:
	case NNTP_TARGET_MESSAGE_ID:
	case NNTP_TARGET_GROUP_MESSAGE_ID:
	{
		unsigned char *header_entries;

		header_entries = get_nntp_header_entries();
		if (!*header_entries) break;

		add_to_string(html, "<pre>");

		while (*header_entries) {
			unsigned char *entry, *value;

			entry = get_next_path_filename(&header_entries, ',');
			if (!entry) continue;

			value = parse_header(conn->cached->head, entry, NULL);
			if (!value) {
				mem_free(entry);
				continue;
			}

			add_format_to_string(html, "<b>%s</b>: ", entry);
			add_html_to_string(html, value, strlen(value));
			add_char_to_string(html, '\n');
			mem_free(value);
			mem_free(entry);
		}

		add_to_string(html, "<hr />");
		break;
	}
	case NNTP_TARGET_ARTICLE_RANGE:
	case NNTP_TARGET_GROUP:
	case NNTP_TARGET_GROUPS:
		add_format_to_string(html,
			"<h2>%s</h2>\n"
			"<hr />\n"
			"<dl>",
			empty_string_or_(title));
		break;

	case NNTP_TARGET_QUIT:
		break;
	}

	mem_free_if(title);
}

static void
add_nntp_html_end(struct string *html, struct connection *conn)
{
	struct nntp_connection_info *nntp = conn->info;

	switch (nntp->target) {
	case NNTP_TARGET_ARTICLE_NUMBER:
	case NNTP_TARGET_MESSAGE_ID:
	case NNTP_TARGET_GROUP_MESSAGE_ID:
		add_to_string(html, "</pre>");
		break;

	case NNTP_TARGET_ARTICLE_RANGE:
	case NNTP_TARGET_GROUP:
	case NNTP_TARGET_GROUPS:
		add_to_string(html, "</dl>");
		break;

	case NNTP_TARGET_QUIT:
		break;
	}

	add_to_string(html, "\n<hr />\n</body>\n</html>");
}

static void
add_nntp_html_line(struct string *html, struct connection *conn,
		   unsigned char *line)
{
	struct nntp_connection_info *nntp = conn->info;

	switch (nntp->target) {
	case NNTP_TARGET_ARTICLE_NUMBER:
	case NNTP_TARGET_MESSAGE_ID:
	case NNTP_TARGET_GROUP_MESSAGE_ID:
		add_html_to_string(html, line, strlen(line));
		break;

	case NNTP_TARGET_ARTICLE_RANGE:
	case NNTP_TARGET_GROUP:
	case NNTP_TARGET_GROUPS:
	{
		unsigned char *desc = strchr(line, '\t');

		if (desc) {
			*desc++ = 0;
		} else {
			desc = "";
		}

		add_format_to_string(html, "<dt><a href=\"%s%s\">%s</a></dt><dd>%s</dd>\n",
			struri(conn->uri), line, line, desc);
		break;
	}
	case NNTP_TARGET_QUIT:
		break;
	}

	add_char_to_string(html, '\n');
}

enum connection_state
read_nntp_response_data(struct connection *conn, struct read_buffer *rb)
{
	struct string html;
	unsigned char *end;
	enum connection_state state = S_TRANS;

	if (conn->from == 0) {
		switch (init_nntp_header(conn, rb)) {
		case S_OK:
			break;

		case S_OUT_OF_MEM:
			return S_OUT_OF_MEM;

		case S_TRANS:
			return S_TRANS;

		default:
			return S_NNTP_ERROR;
		}
	}

	if (!init_string(&html))
		return S_OUT_OF_MEM;

	if (conn->from == 0)
		add_nntp_html_start(&html, conn);

	while ((end = get_nntp_line_end(rb->data, rb->length))) {
		unsigned char *line = check_nntp_line(rb->data, end);

		if (!line) {
			state = S_OK;
			break;
		}

		add_nntp_html_line(&html, conn, line);

		conn->received += end - rb->data;
		kill_buffer_data(rb, end - rb->data);
	}

	if (state != S_TRANS)
		add_nntp_html_end(&html, conn);

	add_fragment(conn->cached, conn->from, html.source, html.length);

	conn->from += html.length;
	done_string(&html);

	return state;
}

/* Interpret response code parameters for code 211 - after GROUP command */
/* The syntax is: 211 <articles> <first-article> <last-article> <name> */
/* Returns 1 on success and 0 on failure */
static int
parse_nntp_group_parameters(struct nntp_connection_info *nntp,
			    unsigned char *pos, unsigned char *end)
{
	errno = 0;

	/* Get <articles> */
	while (pos < end && !isdigit(*pos))
		pos++;

	nntp->articles = strtol(pos, (char **) &pos, 10);
	if (errno || pos >= end || nntp->articles < 0)
		return 0;

	if (nntp->target == NNTP_TARGET_ARTICLE_RANGE)
		return 1;

	/* Get <first-article> */
	while (pos < end && !isdigit(*pos))
		pos++;

	nntp->current_article = strtol(pos, (char **) &pos, 10);
	if (errno || pos >= end || nntp->current_article < 0)
		return 0;

	/* Get <last-article> */
	while (pos < end && !isdigit(*pos))
		pos++;

	nntp->end_article = strtol(pos, (char **) &pos, 10);
	if (errno || pos >= end || nntp->end_article < 0)
		return 0;

	return 1;
}

enum nntp_code
get_nntp_response_code(struct connection *conn, struct read_buffer *rb)
{
	struct nntp_connection_info *nntp = conn->info;
	unsigned char *line = rb->data;
	unsigned char *end = get_nntp_line_end(rb->data, rb->length);
	enum nntp_code code;
	int linelen;

	if (!end) return NNTP_CODE_NONE;

	/* Just to be safe NUL terminate the line */
	end[-1] = 0;

	linelen = end - line;

	if (linelen < sizeof("xxx\r\n") - 1
	    || !isdigit(line[0])
	    || !isdigit(line[1])
	    || !isdigit(line[2])
	    ||  isdigit(line[3]))
		return NNTP_CODE_INVALID;

	code = atoi(line);

	if (!check_nntp_code_valid(code))
		return NNTP_CODE_INVALID;

	/* Only when listing all articles in group the parameters is needed */
	if (code == NNTP_CODE_211_GROUP_SELECTED
	    && nntp->target == NNTP_TARGET_GROUP
	    && !parse_nntp_group_parameters(nntp, line + 4, end))
		return NNTP_CODE_INVALID;

	/* Remove the response line */
	kill_buffer_data(rb, linelen);

	conn->received += linelen;

	return code;
}
