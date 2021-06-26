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
#include "ecmascript/spidermonkey/document.h"
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

		if (isspace(*html) && !html_is_preformatted()) {
			char *h = html;

			while (h < eof && isspace(*h))
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
			if (*(html - 1) == ' ') {	/* Do not replace with isspace() ! --Zas */
				/* BIG performance win; not sure if it doesn't cause any bug */
				if (html < eof && !isspace(*html)) {
					noupdate = 1;
					continue;
				}
				put_chrs(html_context, base_pos, html - base_pos);
			} else {
				put_chrs(html_context, base_pos, html - base_pos - 1);
				put_chrs(html_context, " ", 1);
			}

skip_w:
			while (html < eof && isspace(*html))
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
			if (*html >= ' ' || isspace(*html) || html >= eof) {
				char *dots = fmem_alloc(dotcounter);

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
			if (ee < eof && isspace(*ee)) {
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

static bool
dump_dom_structure(struct source_renderer *renderer, xmlpp::Node *node, int depth)
{
	if (depth >= HTML_MAX_DOM_STRUCTURE_DEPTH) {
		return false;
	}

	std::string tag_name = node->get_name();
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
				std::string v = text->get_content();
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

void
render_xhtml_document(struct cache_entry *cached, struct document *document, struct string *buffer)
{
	struct html_context *html_context;
	struct part *part = NULL;

	part = mem_calloc(1, sizeof(*part));
	if (!part) {
		return;
	}

	part->document = document;
	part->box.x = 0;
	part->box.y = 0;
	part->cx = -1;
	part->cy = 0;
	part->link_num = 0;

	char *start;
	char *end;
	struct string title;
	struct string head;
	struct string tt;

	struct source_renderer renderer;

	assert(cached && document);
	if_assert_failed return;

	if (!init_string(&head)) return;

	if (cached->head) add_to_string(&head, cached->head);

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	struct tag *saved_last_tag_to_move = renderer_context.last_tag_to_move;
	int saved_empty_format = renderer_context.empty_format;
///	int saved_margin = html_context->margin;
	int saved_last_link_to_move = renderer_context.last_link_to_move;


	xmlpp::Document *doc = document->dom;

	if (!buffer) {
		std::string text = doc->write_to_string_formatted();
		init_string(&tt);
		add_bytes_to_string(&tt, text.c_str(), text.size());
		buffer = &tt;
	}
	start = buffer->source;
	end = buffer->source + buffer->length;

	html_context = init_html_parser(cached->uri, &document->options,
	                                start, end, &head, &title,
	                                put_chars_conv, line_break,
	                                html_special);
	if (!html_context) return;

	if (document) {
		struct node *node = mem_alloc(sizeof(*node));

		if (node) {
			int node_width = INT_MAX ;///!html_context->table_level ? INT_MAX : width;

			set_box(&node->box, 0 /*x*/, 0 /*y*/, node_width, 1);
			add_to_list(document->nodes, node);
		}

		renderer_context.last_link_to_move = document->nlinks;
		renderer_context.last_tag_to_move = (struct tag *) &document->tags;
		renderer_context.last_tag_for_newline = (struct tag *) &document->tags;
	} else {
		renderer_context.last_link_to_move = 0;
		renderer_context.last_tag_to_move = (struct tag *) NULL;
		renderer_context.last_tag_for_newline = (struct tag *) NULL;
	}

	renderer_context.g_ctrl_num = 0;
	renderer_context.cached = cached;
	renderer_context.convert_table = get_convert_table(head.source,
							   document->options.cp,
						   document->options.assume_cp,
							   &document->cp,
							   &document->cp_status,
							   document->options.hard_assume);

#ifdef CONFIG_UTF8
	html_context->options->utf8 = is_cp_utf8(document->options.cp);
#endif /* CONFIG_UTF8 */
	html_context->doc_cp = document->cp;

	if (title.length) {
		/* CSM_DEFAULT because init_html_parser() did not
		 * decode entities in the title.  */
		document->title = convert_string(renderer_context.convert_table,
						 title.source, title.length,
						 document->options.cp,
						 CSM_DEFAULT, NULL, NULL, NULL);
	}
	done_string(&title);

	xmlpp::Element *root = doc->get_root_node();
	renderer.html_context = html_context;

	html_context->margin = par_elformat.leftmargin + par_elformat.blockquote_level * (html_context->table_level == 0);

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
