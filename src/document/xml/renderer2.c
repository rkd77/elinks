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
#include "document/html/internal.h"
#include "document/html/parser.h"
#include "document/html/parser/parse.h"
#include "document/plain/renderer.h"
#include "document/renderer.h"
#include "document/xml/renderer2.h"
#include "document/xml/tags.h"
#include "globhist/globhist.h"
#include "intl/charsets.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "terminal/color.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

#include <libxml++/libxml++.h>

static void
dump_text(struct source_renderer *renderer, unsigned char *html, int length)
{
	struct html_context *html_context = renderer->html_context;
	unsigned char *eof = html + length;
	unsigned char *base_pos = html;

	if (length > 0 && (eof[-1] == '\n')) {
		length--;
		eof--;
	}
#ifdef CONFIG_CSS
	if (html_context->was_style) {
///		css_parse_stylesheet(&html_context->css_styles, html_context->base_href, html, eof);
		return;
	}
#endif
//fprintf(stderr, "html='%s'\n", html);
	if (length <= 0) {
		return;
	}
	int noupdate = 0;

main_loop:
	if (!html_is_preformatted()) {
		unsigned char *buffer = fmem_alloc(length+1);
		unsigned char *dst;
		html_context->part = renderer->part;
		html_context->eoff = eof;

		if (!buffer) {
			return;
		}
		dst = buffer;
		*dst = *html;

		for (html++; html <= eof; html++) {
			if (isspace(*html)) {
				if (*dst != ' ') {
					*(++dst) = ' ';
				}
			} else {
				*(++dst) = *html;
			}
		}

		if (dst - buffer > 1) {
			if (*buffer == ' ') {
				html_context->putsp = HTML_SPACE_ADD;
			}
		}
		put_chrs(html_context, buffer, dst - buffer);
		fmem_free(buffer);
	}

	while (html < eof) {
		int dotcounter = 0;

		if (!noupdate) {
			html_context->part = renderer->part;
			html_context->eoff = eof;
			base_pos = html;
		} else {
			noupdate = 0;
		}
		if (html_is_preformatted()) {
			html_context->putsp = HTML_SPACE_NORMAL;
			if (*html == ASCII_TAB) {
				put_chrs(html_context, base_pos, html - base_pos);
				put_chrs(html_context, "        ",
				8 - (html_context->position % 8));
				html++;
				continue;

			} else if (*html == ASCII_CR || *html == ASCII_LF) {
				put_chrs(html_context, base_pos, html - base_pos);
				if (html - base_pos == 0 && html_context->line_breax > 0) {
					html_context->line_breax--;
				}
next_break:
				if (*html == ASCII_CR && html < eof - 1 && html[1] == ASCII_LF) {
					html++;
				}
				ln_break(html_context, 1);
				html++;

				if (*html == ASCII_CR || *html == ASCII_LF) {
					html_context->line_breax = 0;
					goto next_break;
				}
				continue;

			} else if (html + 5 < eof && *html == '&') {
				/* Really nasty hack to make &#13; handling in
				 * <pre>-tags lynx-compatible. It works around
				 * the entity handling done in the renderer,
				 * since checking #13 value there would require
				 * something along the lines of NBSP_CHAR or
				 * checking for '\n's in AT_PREFORMATTED text. */
				/* See bug 52 and 387 for more info. */
				int length = html - base_pos;
				int newlines;

				html = (unsigned char *) count_newline_entities(html, eof, &newlines);
				if (newlines) {
					put_chrs(html_context, base_pos, length);
					ln_break(html_context, newlines);
					continue;
				}
			}
		}

		while (*html < ' ') {
			if (html - base_pos) {
				put_chrs(html_context, base_pos, html - base_pos);
			}

			dotcounter++;
			base_pos = ++html;
			if (*html >= ' ' || isspace(*html) || html >= eof) {
				unsigned char *dots = fmem_alloc(dotcounter);

				if (dots) {
					memset(dots, '.', dotcounter);
					put_chrs(html_context, dots, dotcounter);
					fmem_free(dots);
				}
				goto main_loop;
			}
		}
	}
}

#define HTML_MAX_DOM_STRUCTURE_DEPTH 1024

static bool
dump_dom_structure(struct source_renderer *renderer, xmlpp::Node *node, int depth)
{
	if (depth >= HTML_MAX_DOM_STRUCTURE_DEPTH) {
		return false;
	}

	std::string tag_name = node->get_name();
	struct element_info2 *ei = get_tag_value(tag_name.c_str(), tag_name.size());

	/* Print this node's entry */
	if (ei) {
		ei->open(renderer, node, NULL, NULL, NULL, NULL);
	}

	/* Get the node's first child */
	auto children = node->get_children();
	auto it = children.begin();
	auto end = children.end();

	for (;it != end; ++it) {
		xmlpp::Element *el = dynamic_cast<xmlpp::Element *>(*it);

		if (el) {
			dump_dom_structure(renderer, el, depth + 1);
		} else {
			xmlpp::TextNode *text = dynamic_cast<xmlpp::TextNode *>(*it);

			if (text) {
				std::string v = text->get_content();
				dump_text(renderer, v.c_str(), v.size());
			}
		}
	}
	if (ei && ei->close) {
		ei->close(renderer, node, NULL, NULL, NULL, NULL);
	}

	return true;
}

void
render_xhtml_document(struct cache_entry *cached, struct document *document, struct string *buffer)
{
	struct html_context *html_context;
	struct part *part = NULL;
	char *start;
	char *end;
	struct string title;
	struct string head;

	struct source_renderer renderer;

	assert(cached && document);
	if_assert_failed return;

	if (!init_string(&head)) return;

	if (cached->head) add_to_string(&head, cached->head);

	start = buffer->source;
	end = buffer->source + buffer->length;

	html_context = init_html_parser(cached->uri, &document->options,
	                                start, end, &head, &title,
	                                put_chars_conv, line_break,
	                                html_special);
	if (!html_context) return;

///	renderer_context.g_ctrl_num = 0;
///	renderer_context.cached = cached;
///	renderer_context.convert_table = get_convert_table(head.source,
///							   document->options.cp,
///							   document->options.assume_cp,
///							   &document->cp,
///							   &document->cp_status,
///							   document->options.hard_assume);

	struct conv_table *convert_table = get_convert_table(head.source,
							   document->options.cp,
							   document->options.assume_cp,
							   &document->cp,
							   &document->cp_status,
							   document->options.hard_assume);;



#ifdef CONFIG_UTF8
	html_context->options->utf8 = is_cp_utf8(document->options.cp);
#endif /* CONFIG_UTF8 */
	html_context->doc_cp = document->cp;

	if (title.length) {
		/* CSM_DEFAULT because init_html_parser() did not
		 * decode entities in the title.  */
		document->title = convert_string(convert_table,
						 title.source, title.length,
						 document->options.cp,
						 CSM_DEFAULT, NULL, NULL, NULL);
	}
	done_string(&title);

	xmlpp::Document *doc = document->dom;
	xmlpp::Element *root = doc->get_root_node();
	renderer.html_context = html_context;

	html_context->putsp = HTML_SPACE_SUPPRESS;
	html_context->line_breax = html_context->table_level ? 2 : 1;
	html_context->position = 0;
	html_context->was_br = 0;
	html_context->was_li = 0;
	html_context->was_body = 0;
/*	html_context->was_body_background = 0; */
	renderer.part = html_context->part = part;
///	html_context->eoff = eof;

	dump_dom_structure(&renderer, root, 0);

///	part = format_html_part(html_context, start, end, par_format.align,
///			        par_format.leftmargin + par_format.blockquote_level * (html_context->table_level == 0),
///				document->options.document_width, document,
///			        0, 0, head.source, 1);

	/* Drop empty allocated lines at end of document if any
	 * and adjust document height. */
	while (document->height && !document->data[document->height - 1].length)
		mem_free_if(document->data[--document->height].chars);

	/* Calculate document width. */
	{
		int i;

		document->width = 0;
		for (i = 0; i < document->height; i++)
			int_lower_bound(&document->width, document->data[i].length);
	}

#if 1
	document->options.needs_width = 1;
#else
	/* FIXME: This needs more tuning since if we are centering stuff it
	 * does not work. */
	document->options.needs_width =
				(document->width + (document->options.margin
				 >= document->options.width));
#endif

	document->color.background = par_elformat.color.background;

	done_html_parser(html_context);

	/* Drop forms which has been serving as a placeholder for form items
	 * added in the wrong order due to the ordering of table rendering. */
	{
		struct form *form;

		foreach (form, document->forms) {
			if (form->form_num)
				continue;

			if (list_empty(form->items))
				done_form(form);

			break;
		}
	}

	/* @part was residing in html_context so it has to stay alive until
	 * done_html_parser(). */
	done_string(&head);
	mem_free_if(part);

#if 0 /* debug purpose */
	{
		FILE *f = fopen("forms", "ab");
		struct el_form_control *form;
		char *qq;
		fprintf(f,"FORM:\n");
		foreach (form, document->forms) {
			fprintf(f, "g=%d f=%d c=%d t:%d\n",
				form->g_ctrl_num, form->form_num,
				form->ctrl_num, form->type);
		}
		fprintf(f,"fragment: \n");
		for (qq = start; qq < end; qq++) fprintf(f, "%c", *qq);
		fprintf(f,"----------\n\n");
		fclose(f);
	}
#endif
}
