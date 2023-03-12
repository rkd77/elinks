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

#ifndef CONFIG_LIBDOM
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

	if (!node) {
		return;
	}

	if (!start) {
		const auto textNode = dynamic_cast<const xmlpp::TextNode*>(node);

		if (textNode) {
			add_bytes_to_string(buf, textNode->get_content().c_str(), textNode->get_content().length());
		} else {
			const auto cDataNode = dynamic_cast<const xmlpp::CdataNode*>(node);

			if (cDataNode) {
				add_bytes_to_string(buf, cDataNode->get_content().c_str(), cDataNode->get_content().length());
			} else {
				auto element = dynamic_cast<xmlpp::Element*>(node);

				if (element) {
					dump_element(mapa, buf, element);
				}
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
	(void)get_convert_table(cached->head ?: (char *)"", document->options.cp,
					  document->options.assume_cp,
					  &document->cp,
					  &document->cp_status,
					  document->options.hard_assume);

		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return;
	}
	assert(cached && document);
	if_assert_failed return;

	xmlpp::Document *doc = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)doc->get_root_node();

	if (!buffer) {
		struct string tt;

		if (!init_string(&tt)) {
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
	render_html_document(cached, document, buffer);
}
#endif
