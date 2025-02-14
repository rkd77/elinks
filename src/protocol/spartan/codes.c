/* Spartan response codes */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* Needed for asprintf() */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "intl/libintl.h"
#include "network/connection.h"
#include "protocol/spartan/codes.h"
#include "protocol/spartan/spartan.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/snprintf.h"
#include "viewer/text/draw.h"

static const char *
spartan_code_to_string(int code)
{
	switch (code) {
		case 2:
			return "OK";
		case 3:
			return "REDIRECT";
		case 4:
			return "CLIENT ERROR";
		case 5:
			return "SERVER ERROR";
		default:
			return "UNKNOWN ERROR";
	}
}

static char *
get_spartan_error_document(struct terminal *term, struct uri *uri, const char *msg, int code)
{
	const char *codestr = spartan_code_to_string(code);
	char *title = asprintfa(_("spartan error %d", term), code);
	struct string string;

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
		"  %s</p>\n"
		"  <p>\n"
		"  URI: <a href=\"%s\">%s</a>\n", msg, struri(uri), struri(uri));
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

static void
show_spartan_error_document(struct session *ses, void *data)
{
	struct spartan_error_info *info = (struct spartan_error_info *)data;
	struct terminal *term = ses->tab->term;
	struct cache_entry *cached = find_in_cache(info->uri);
	struct cache_entry *cache = cached ? cached : get_cache_entry(info->uri);
	char *str = NULL;

	if (cache) str = get_spartan_error_document(term, info->uri, info->msg, info->code);

	if (str) {
		/* The codepage that _("foo", term) used when it was
		 * called by get_spartan_error_document.  */
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
	mem_free_if(info->msg);
	done_uri(info->uri);
	mem_free(info);
}


void
spartan_error_document(struct connection *conn, const char *msg, int code)
{
	struct spartan_error_info *info;

	assert(conn && conn->uri);

	info = (struct spartan_error_info *)mem_calloc(1, sizeof(*info));
	if (!info) return;

	info->code = code;
	info->uri = get_uri_reference(conn->uri);
	info->msg = null_or_stracpy(msg);

	add_questions_entry(show_spartan_error_document, info);
}
