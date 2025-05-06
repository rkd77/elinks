/* Plain text document renderer */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bookmarks/bookmarks.h"
#include "cache/cache.h"
#include "config/options.h"
#include "document/docdata.h"
#include "document/document.h"
#include "document/format.h"
#include "document/options.h"
#include "document/gemini/renderer.h"
#include "document/html/renderer.h"
#include "document/renderer.h"
#include "globhist/globhist.h"
#include "intl/charsets.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "terminal/color.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

#include <stdio.h>

static void
convert_single_line(struct string *ret, struct string *line)
{
	ELOG
	if (line->length >= 3 && !strncmp(line->source, "###", 3)) {
		add_to_string(ret, "<h3>");
		add_html_to_string(ret, line->source + 3, line->length - 3);
		add_to_string(ret, "</h3>");
		return;
	}

	if (line->length >= 2 && !strncmp(line->source, "##", 2)) {
		add_to_string(ret, "<h2>");
		add_html_to_string(ret, line->source + 2, line->length - 2);
		add_to_string(ret, "</h2>");
		return;
	}

	if (line->length >= 1 && !strncmp(line->source, "#", 1)) {
		add_to_string(ret, "<h1>");
		add_html_to_string(ret, line->source + 1, line->length - 1);
		add_to_string(ret, "</h1>");
		return;
	}

	if (line->length >= 2 && !strncmp(line->source, "*", 1)) {
		add_to_string(ret, "<li>");
		add_html_to_string(ret, line->source + 1, line->length - 1);
		add_to_string(ret, "</li>");
		return;
	}

	if (line->length >= 1 && !strncmp(line->source, ">", 1)) {
		add_to_string(ret, "<blockquote>");
		add_html_to_string(ret, line->source + 1, line->length - 1);
		add_to_string(ret, "</blockquote>");
		return;
	}

	if (line->length >= 2 && !strncmp(line->source, "=>", 2)) {
		int i = 2;
		int href;
		int inner;
		add_to_string(ret, "<a href=\"");
		for (; i < line->length; ++i) {
			if (line->source[i] != ' ' && line->source[i] != '\t') {
				break;
			};
		}
		href = i;

		for (; i < line->length; ++i) {
			if (line->source[i] == ' ' || line->source[i] == '\t') {
				break;
			}
		}

		add_bytes_to_string(ret, line->source + href, i - href);
		add_to_string(ret, "\">");

		inner = i;

		for (; i < line->length; ++i) {
			if (line->source[i] != ' ' && line->source[i] != '\t') {
				break;
			};
		}

		if (inner == i) {
			add_bytes_to_string(ret, line->source + href, i - href);
		} else {
			add_html_to_string(ret, line->source + i, line->length - i);
		}
		add_to_string(ret, "</a><br/>");
		return;
	}

	if (line->length >= 2 && !strncmp(line->source, "=:", 2)) {
		int i = 2;
		int href;
		int inner;

		struct string form;
		if (!init_string(&form)) {
			return;
		}
		add_to_string(&form, "<form action=\"");
		for (; i < line->length; ++i) {
			if (line->source[i] != ' ' && line->source[i] != '\t') {
				break;
			};
		}
		href = i;

		for (; i < line->length; ++i) {
			if (line->source[i] == ' ' || line->source[i] == '\t') {
				break;
			}
		}

		add_bytes_to_string(&form, line->source + href, i - href);
		add_to_string(&form, "\" method=\"get\"><textarea name=\"elq\" cols=\"10\" rows=\"1\"></textarea><input type=\"submit\"/></form>");

		inner = i;

		for (; i < line->length; ++i) {
			if (line->source[i] != ' ' && line->source[i] != '\t') {
				break;
			};
		}

		if (inner == i) {
			add_bytes_to_string(ret, line->source + href, i - href);
		} else {
			add_html_to_string(ret, line->source + i, line->length - i);
		}
		add_string_to_string(ret, &form);
		done_string(&form);
		return;
	}
	add_html_to_string(ret, line->source, line->length);
	add_to_string(ret, "<br/>");
}

void
render_gemini_document(struct cache_entry *cached, struct document *document,
		      struct string *buffer)
{
	ELOG
	int preformat = 0;
	int in_list = 0;
	int i = 0;
	int begin = 0;
	struct string pre_start = INIT_STRING("<pre>", 5);
	struct string pre_end = INIT_STRING("</pre>", 6);
	struct string gem_pre = INIT_STRING("```", 3);
	struct string html;
	char *uristring;

	char *head = empty_string_or_(cached->head);

	(void)get_convert_table(head, document->options.cp,
					  document->options.assume_cp,
					  &document->cp,
					  &document->cp_status,
					  document->options.hard_assume);

	if (!init_string(&html)) {
		return;
	}
	uristring = get_uri_string(document->uri, URI_PUBLIC);

	add_to_string(&html, "<html><head><meta charset=\"utf-8\"/><base href=\"");
	add_to_string(&html, uristring);
	add_to_string(&html, "\"/></head><body>");
	mem_free_if(uristring);

	while (i < buffer->length) {

		for (i = begin; i < buffer->length; ++i) {
			if (buffer->source[i] == 13 || buffer->source[i] == 10) break;
		}

		if (begin == i) {
			add_to_string(&html, "</p><p>");
		}

		if (begin < i) {
			int len = i - begin;

			struct string line;
			line.source = buffer->source + begin;
			line.length = len;
			struct string *repl;

			if (len >= 3 && (!strncmp(line.source, "```", 3)
				|| !strncmp(line.source + len - 3, "```", 3))) {
				preformat = !preformat;
				repl = preformat ? &pre_start : &pre_end;
				el_string_replace(&html, &line, &gem_pre, repl);
				if (preformat) {
					add_char_to_string(&html, '\n');
				}
			} else if (preformat) {
				add_string_to_string(&html, &line);
				add_char_to_string(&html, '\n');
			} else {
				struct string html_line;

				if (init_string(&html_line)) {
					convert_single_line(&html_line, &line);

					if (html_line.length >= 4
					&& !strncmp(html_line.source, "<li>", 4)) {
						if (!in_list) {
							in_list = 1;
							add_to_string(&html, "<ul>\n");
							add_string_to_string(&html, &html_line);
						} else {
							add_string_to_string(&html, &html_line);
						}
					} else if (in_list) {
						in_list = 0;
						add_to_string(&html, "</ul>\n");
						add_string_to_string(&html, &html_line);
					} else {
						add_string_to_string(&html, &html_line);
					}
					done_string(&html_line);
				}
			}
		}
		begin = i + 1;
		if (!preformat) add_to_string(&html, "\n");
	}
	add_to_string(&html, "</body></html>");

	render_html_document(cached, document, &html);
}
