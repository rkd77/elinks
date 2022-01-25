/* Gemini response codes */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* Needed for asprintf() */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "intl/libintl.h"
#include "network/connection.h"
#include "protocol/gemini/codes.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/snprintf.h"
#include "viewer/text/draw.h"


struct gemini_code {
	int num;
	const char *str;
};

static const struct gemini_code gemini_code[] = {
	{ 10, "INPUT" },
	{ 21, "SENSITIVE INPUT" },
	{ 20, "SUCCESS" },
	{ 30, "REDIRECT - TEMPORARY" },
	{ 31, "REDIRECT - PERMANENT" },
	{ 40, "TEMPORARY FAILURE" },
	{ 41, "SERVER UNAVAILABLE" },
	{ 42, "CGI ERROR" },
	{ 43, "PROXY ERROR" },
	{ 44, "SLOW DOWN" },
	{ 50, "PERMANENT FAILURE" },
	{ 51, "NOT FOUND" },
	{ 52, "GONE" },
	{ 53, "PROXY REQUEST REFUSED" },
	{ 59, "BAD REQUEST" },
	{ 60, "CLIENT CERTIFICATE REQUIRED" },
	{ 61, "CERTIFICATE NOT AUTHORISED" },
	{ 62, "CERTIFICATE NOT VALID" },
};

static int
compare_gemini_codes(const void *key, const void *element)
{
	int first = (long) key;
	int second = ((const struct gemini_code *) element)->num;

	return first - second;
}

static const char *
gemini_code_to_string(int code)
{
	const struct gemini_code *element
		= (const struct gemini_code *)bsearch((void *) (long) code, gemini_code,
			  sizeof_array(gemini_code),
			  sizeof(*element),
			  compare_gemini_codes);

	if (element) return element->str;

	return NULL;
}

static char *
get_gemini_error_document(struct terminal *term, struct uri *uri, int code)
{
	const char *codestr = gemini_code_to_string(code);
	char *title = asprintfa(_("Gemini error %02d", term), code);
	struct string string;

	if (!codestr) codestr = "UNKNOWN ERROR";

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
		"  requested.\n",
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

struct gemini_error_info {
	int code;
	struct uri *uri;
};

static void
show_gemini_error_document(struct session *ses, void *data)
{
	struct gemini_error_info *info = (struct gemini_error_info *)data;
	struct terminal *term = ses->tab->term;
	struct cache_entry *cached = find_in_cache(info->uri);
	struct cache_entry *cache = cached ? cached : get_cache_entry(info->uri);
	char *str = NULL;

	if (cache) str = get_gemini_error_document(term, info->uri, info->code);

	if (str) {
		/* The codepage that _("foo", term) used when it was
		 * called by get_gemini_error_document.  */
		const int gettext_codepage = get_terminal_codepage(term);

		if (cached) delete_entry_content(cache);

		/* If we run out of memory here, it's perhaps better
		 * to display a malformatted error message than none
		 * at all.  */
		mem_free_set(&cache->content_type, stracpy("text/html"));
		mem_free_set(&cache->head,
			     straconcat("\r\nContent-Type: text/html; charset=",
					get_cp_mime_name(gettext_codepage),
					"\r\n", (char *) NULL));
		add_fragment(cache, 0, str, strlen(str));
		mem_free(str);

		draw_formatted(ses, 1);
	}

	done_uri(info->uri);
	mem_free(info);
}


void
gemini_error_document(struct connection *conn, int code)
{
	struct gemini_error_info *info;

	assert(conn && conn->uri);

	info = (struct gemini_error_info *)mem_calloc(1, sizeof(*info));
	if (!info) return;

	info->code = code;
	info->uri = get_uri_reference(conn->uri);

	add_questions_entry(show_gemini_error_document, info);
}
