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
#ifdef CONFIG_ECMASCRIPT
#include "ecmascript/ecmascript.h"
#endif
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
#include <map>


#if 0

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
	while (html < eof) {
		char *name, *attr, *end;
		int namelen, endingtag;
		int dotcounter = 0;

		if (!noupdate) {
			html_context->part = renderer->part;
			html_context->eoff = eof;
			base_pos = html;
		} else {
			noupdate = 0;
		}

		if (isspace((unsigned char)*html) && !html_is_preformatted()) {
			char *h = html;

			while (h < eof && isspace((unsigned char)*h))
				h++;
			if (h + 1 < eof && h[0] == '<' && h[1] == '/') {
				if (!parse_element(h, eof, &name, &namelen, &attr, &end)) {
					put_chrs(html_context, base_pos, html - base_pos);
					base_pos = html = h;
					html_context->putsp = HTML_SPACE_ADD;
					goto element;
				}
			}
			html++;
			if (!(html_context->position + (html - base_pos - 1)))
				goto skip_w; /* ??? */
			if (*(html - 1) == ' ') {	/* Do not replace with isspace((unsigned char)) ! --Zas */
				/* BIG performance win; not sure if it doesn't cause any bug */
				if (html < eof && !isspace((unsigned char)*html)) {
					noupdate = 1;
					continue;
				}
				put_chrs(html_context, base_pos, html - base_pos);
			} else {
				put_chrs(html_context, base_pos, html - base_pos - 1);
				put_chrs(html_context, " ", 1);
			}

skip_w:
			while (html < eof && isspace((unsigned char)*html))
				html++;
			continue;
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
				if (html - base_pos == 0 && html_context->line_breax > 0)
					html_context->line_breax--;
next_break:
				if (*html == ASCII_CR && html < eof - 1
				    && html[1] == ASCII_LF)
					html++;
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

				html = (char *) count_newline_entities(html, eof, &newlines);
				if (newlines) {
					put_chrs(html_context, base_pos, length);
					ln_break(html_context, newlines);
					continue;
				}
			}
		}

		while ((unsigned char)*html < ' ') {
			if (html - base_pos)
				put_chrs(html_context, base_pos, html - base_pos);

			dotcounter++;
			base_pos = ++html;
			if (*html >= ' ' || isspace((unsigned char)*html) || html >= eof) {
				char *dots = (char *)fmem_alloc(dotcounter);

				if (dots) {
					memset(dots, '.', dotcounter);
					put_chrs(html_context, dots, dotcounter);
					fmem_free(dots);
				}
				goto main_loop;
			}
		}

		if (html + 2 <= eof && html[0] == '<' && (html[1] == '!' || html[1] == '?')
		    && !(html_context->was_xmp || html_context->was_style)) {
			put_chrs(html_context, base_pos, html - base_pos);
			html = skip_comment(html, eof);
			continue;
		}

		if (*html != '<' || parse_element(html, eof, &name, &namelen, &attr, &end)) {
			html++;
			noupdate = 1;
			continue;
		}

element:
		endingtag = *name == '/'; name += endingtag; namelen -= endingtag;
		if (!endingtag && html_context->putsp == HTML_SPACE_ADD && !html_top->invisible)
			put_chrs(html_context, " ", 1);
		put_chrs(html_context, base_pos, html - base_pos);
		if (!html_is_preformatted() && !endingtag && html_context->putsp == HTML_SPACE_NORMAL) {
			char *ee = end;
			char *nm;

///		while (!parse_element(ee, eof, &nm, NULL, NULL, &ee))
///			if (*nm == '/')
///					goto ng;
			if (ee < eof && isspace((unsigned char)*ee)) {
				put_chrs(html_context, " ", 1);
			}
		}
//ng:
//		html = process_element(name, namelen, endingtag, end, html, eof, attr, html_context);
	}

	if (noupdate) put_chrs(html_context, base_pos, html - base_pos);
	//ln_break(html_context, 1);
	/* Restore the part in case the html_context was trashed in the last
	 * iteration so that when destroying the stack in the caller we still
	 * get the right part pointer. */
	html_context->part = renderer->part;
	html_context->putsp = HTML_SPACE_SUPPRESS;
	html_context->position = 0;
	html_context->was_br = 0;
}

#define HTML_MAX_DOM_STRUCTURE_DEPTH 1024

bool
dump_dom_structure(struct source_renderer *renderer, void *nod, int depth)
{
	xmlpp::Node *node = nod;

	if (depth >= HTML_MAX_DOM_STRUCTURE_DEPTH) {
		return false;
	}

	xmlpp::ustring tag_name = node->get_name();
	struct element_info2 *ei = get_tag_value(tag_name.c_str(), tag_name.size());

	if (!ei) return true;

	start_element_2(ei, renderer, node);

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
				if (renderer->html_context->skip_select || renderer->html_context->skip_textarea) continue;
				xmlpp::ustring v = text->get_content();
				dump_text(renderer, v.c_str(), v.size());
			}
		}
	}
	end_element_2(ei, renderer, node);
/*
	if (ei && ei->close) {
		ei->close(renderer, node, NULL, NULL, NULL, NULL);
	}
*/
	return true;
}
#endif

static void
dump_element(std::map<int, xmlpp::Element *> *mapa, struct string *buf, xmlpp::Element *element)
{
	add_char_to_string(buf, '<');
	(*mapa)[buf->length] = element;

	add_to_string(buf, element->get_name().c_str());
	auto attrs = element->get_attributes();
	auto it = attrs.begin();
	auto end = attrs.end();

	for (;it != end; ++it) {
		add_char_to_string(buf, ' ');
		add_to_string(buf, (*it)->get_name().c_str());
		add_char_to_string(buf, '=');
		add_char_to_string(buf, '"');
		add_to_string(buf, (*it)->get_value().c_str());
		add_char_to_string(buf, '"');
	}
	add_char_to_string(buf, '>');
}

static void
walk_tree(std::map<int, xmlpp::Element *> *mapa, struct string *buf, void *nod, bool start)
{
	xmlpp::Node *node = static_cast<xmlpp::Node *>(nod);

	if (!start) {
		const auto textNode = dynamic_cast<const xmlpp::ContentNode*>(node);

		if (textNode) {
			add_bytes_to_string(buf, textNode->get_content().c_str(), textNode->get_content().length());
		} else {
			auto element = dynamic_cast<xmlpp::Element*>(node);

			if (element) {
				dump_element(mapa, buf, element);
			}
		}
	}

	auto childs = node->get_children();
	auto it = childs.begin();
	auto end = childs.end();

	for (; it != end; ++it) {
		walk_tree(mapa, buf, *it, false);
	}

	if (!start) {
		const auto element = dynamic_cast<const xmlpp::Element*>(node);
		if (element) {
			add_to_string(buf, "</");
			add_to_string(buf, element->get_name().c_str());
			add_char_to_string(buf, '>');
		}
	}
}

void
render_xhtml_document(struct cache_entry *cached, struct document *document, struct string *buffer)
{
	if (!document->dom) {
	(void)get_convert_table(cached->head, document->options.cp,
					  document->options.assume_cp,
					  &document->cp,
					  &document->cp_status,
					  document->options.hard_assume);

		document->dom = document_parse(document);
	}

	if (!document->dom) {
		render_html_document(cached, document, buffer);
		return;
	}
	struct string head;

	assert(cached && document);
	if_assert_failed return;

	if (!init_string(&head)) return;

	bool add_to_head = true;
	if (cached->head) {
		if (!strncmp(cached->head, "\r\nContent-Type: text/html; charset=utf-8\r\n",
		sizeof("\r\nContent-Type: text/html; charset=utf-8\r\n") - 1)) {
			add_to_head = false;
		}
	}

	if (add_to_head) {
		add_to_string(&head, "\r\nContent-Type: text/html; charset=utf-8\r\n");
		if (cached->head) add_to_string(&head, cached->head);
	}

	xmlpp::Document *doc = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)doc->get_root_node();

	if (!buffer) {
		struct string tt;

		if (!init_string(&tt)) {
			done_string(&head);
			return;
		}
		std::map<int, xmlpp::Element *> *mapa = (std::map<int, xmlpp::Element *> *)document->element_map;

		if (!mapa) {
			mapa = new std::map<int, xmlpp::Element *>;
			document->element_map = (void *)mapa;
		} else {
			mapa->clear();
		}

		walk_tree(mapa, &tt, root, true);
		buffer = &tt;
		document->text = tt.source;
	}

	if (add_to_head) {
		mem_free_set(&cached->head, head.source);
	}
	render_html_document(cached, document, buffer);
}
