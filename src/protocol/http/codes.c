/* HTTP response codes */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* Needed for asprintf() */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "intl/gettext/libintl.h"
#include "network/connection.h"
#include "protocol/http/codes.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/snprintf.h"
#include "viewer/text/draw.h"


struct http_code {
	int num;
	const unsigned char *str;
};

/* Source: http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html */
static const struct http_code http_code[] = {
	{ 100, "Continue" },
	{ 101, "Switching Protocols" },
	{ 200, "OK" },
	{ 201, "Created" },
	{ 202, "Accepted" },
	{ 203, "Non-Authoritative Information" },
	{ 204, "No Content" },
	{ 205, "Reset Content" },
	{ 206, "Partial Content" },
	{ 300, "Multiple Choices" },
	{ 301, "Moved Permanently" },
	{ 302, "Found" },
	{ 303, "See Other" },
	{ 304, "Not Modified" },
	{ 305, "Use Proxy" },
	{ 306, "(Unused)" },
	{ 307, "Temporary Redirect" },
	{ 400, "Bad Request" },
	{ 401, "Unauthorized" },
	{ 402, "Payment Required" },
	{ 403, "Forbidden" },
	{ 404, "Not Found" },
	{ 405, "Method Not Allowed" },
	{ 406, "Not Acceptable" },
	{ 407, "Proxy Authentication Required" },
	{ 408, "Request Timeout" },
	{ 409, "Conflict" },
	{ 410, "Gone" },
	{ 411, "Length Required" },
	{ 412, "Precondition Failed" },
	{ 413, "Request Entity Too Large" },
	{ 414, "Request-URI Too Long" },
	{ 415, "Unsupported Media Type" },
	{ 416, "Requested Range Not Satisfiable" },
	{ 417, "Expectation Failed" },
	{ 500, "Internal Server Error" },
	{ 501, "Not Implemented" },
	{ 502, "Bad Gateway" },
	{ 503, "Service Unavailable" },
	{ 504, "Gateway Timeout" },
	{ 505, "HTTP Version Not Supported" },
};

static int
compare_http_codes(const void *key, const void *element)
{
	int first = (long) key;
	int second = ((const struct http_code *) element)->num;

	return first - second;
}

static const unsigned char *
http_code_to_string(int code)
{
	const struct http_code *element
		= bsearch((void *) (long) code, http_code,
			  sizeof_array(http_code),
			  sizeof(*element),
			  compare_http_codes);

	if (element) return element->str;

	return NULL;
}


/* TODO: Some short intermediate document for the 3xx messages? --pasky */
static unsigned char *
get_http_error_document(struct terminal *term, struct uri *uri, int code)
{
	const unsigned char *codestr = http_code_to_string(code);
	unsigned char *title = asprintfa(_("HTTP error %03d", term), code);
	struct string string;

	if (!codestr) codestr = "Unknown error";

	if (!init_string(&string)) {
		mem_free_if(title);
		return NULL;
	}

	add_format_to_string(&string,
		"<html>\n"
		" <head><title>%s</title></head>\n"
		" <body>\n"
		"  <h1 align=\"left\">%s: %s</h1>\n"
#ifndef CONFIG_SMALL
		"  <hr />\n"
		"  <p>\n"
#endif
		, title, title, codestr);

#ifndef CONFIG_SMALL
	add_format_to_string(&string, _(
		"  An error occurred on the server while fetching the document you\n"
		"  requested. However, the server did not send back any explanation of what\n"
		"  happened, so it is unknown what went wrong. Please contact the web\n"
		"  server administrator about this, if you believe that this error should\n"
		"  not occur since it is not a nice behaviour from the web server at all\n"
		"  and indicates that there is some much deeper problem with the web server\n"
		"  software.\n",
		term));

	add_format_to_string(&string,
		"  </p>\n"
		"  <p>\n"
		"  URI: <a href=\"%s\">%s</a>\n", struri(uri), struri(uri));
#endif
	add_format_to_string(&string,
#ifndef CONFIG_SMALL
		" </p>\n"
		" <hr />\n"
#endif
		" </body>\n"
		"</html>\n");

	mem_free_if(title);

	return string.source;
}

struct http_error_info {
	int code;
	struct uri *uri;
};

static void
show_http_error_document(struct session *ses, void *data)
{
	struct http_error_info *info = data;
	struct terminal *term = ses->tab->term;
	struct cache_entry *cached = find_in_cache(info->uri);
	struct cache_entry *cache = cached ? cached : get_cache_entry(info->uri);
	unsigned char *str = NULL;

	if (cache) str = get_http_error_document(term, info->uri, info->code);

	if (str) {
		/* The codepage that _("foo", term) used when it was
		 * called by get_http_error_document.  */
		const int gettext_codepage = get_terminal_codepage(term);

		if (cached) delete_entry_content(cache);

		/* If we run out of memory here, it's perhaps better
		 * to display a malformatted error message than none
		 * at all.  */
		mem_free_set(&cache->content_type, stracpy("text/html"));
		mem_free_set(&cache->head,
			     straconcat("\r\nContent-Type: text/html; charset=",
					get_cp_mime_name(gettext_codepage),
					"\r\n", (unsigned char *) NULL));
		add_fragment(cache, 0, str, strlen(str));
		mem_free(str);

		draw_formatted(ses, 1);
	}

	done_uri(info->uri);
	mem_free(info);
}


void
http_error_document(struct connection *conn, int code)
{
	struct http_error_info *info;

	assert(conn && conn->uri);

	info = mem_calloc(1, sizeof(*info));
	if (!info) return;

	info->code = code;
	info->uri = get_uri_reference(conn->uri);

	add_questions_entry(show_http_error_document, info);
}
