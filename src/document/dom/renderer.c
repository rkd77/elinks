/* DOM document renderer */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "document/document.h"
#include "document/dom/renderer.h"
#include "document/dom/rss.h"
#include "document/dom/source.h"
#include "document/dom/util.h"
#include "document/renderer.h"
#include "dom/configuration.h"
#include "dom/scanner.h"
#include "dom/sgml/parser.h"
#include "dom/sgml/html/html.h"
#include "dom/sgml/rss/rss.h"
#include "dom/node.h"
#include "dom/stack.h"
#include "intl/charsets.h"
#include "protocol/uri.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


static inline void
init_dom_renderer(struct dom_renderer *renderer, struct document *document,
		  struct string *buffer, struct conv_table *convert_table)
{
	memset(renderer, 0, sizeof(*renderer));

	renderer->document	= document;
	renderer->convert_table = convert_table;
	renderer->convert_mode	= document->options.plain ? CSM_NONE : CSM_DEFAULT;
	renderer->source	= buffer->source;
	renderer->end		= buffer->source + buffer->length;
	renderer->position	= renderer->source;
	renderer->base_uri	= get_uri_reference(document->uri);
}

static inline void
done_dom_renderer(struct dom_renderer *renderer)
{
	done_uri(renderer->base_uri);
}


static void
get_doctype(struct dom_renderer *renderer, struct cache_entry *cached)
{
	if (!c_strcasecmp("application/rss+xml", cached->content_type)) {
		renderer->doctype = SGML_DOCTYPE_RSS;

	} else if (!c_strcasecmp("application/docbook+xml",
				 cached->content_type)) {
		renderer->doctype = SGML_DOCTYPE_DOCBOOK;

	} else if (!c_strcasecmp("application/xbel+xml", cached->content_type)
		   || !c_strcasecmp("application/x-xbel", cached->content_type)
		   || !c_strcasecmp("application/xbel", cached->content_type)) {
		renderer->doctype = SGML_DOCTYPE_XBEL;

	} else {
		assertm(!c_strcasecmp("text/html", cached->content_type)
			|| !c_strcasecmp("application/xhtml+xml",
					 cached->content_type),
			"Couldn't resolve doctype '%s'", cached->content_type);

		renderer->doctype = SGML_DOCTYPE_HTML;
	}
}

/* Shared multiplexor between renderers */
void
render_dom_document(struct cache_entry *cached, struct document *document,
		    struct string *buffer)
{
	unsigned char *head = empty_string_or_(cached->head);
	struct dom_renderer renderer;
	struct dom_config config;
	struct conv_table *convert_table;
	struct sgml_parser *parser;
 	enum sgml_parser_type parser_type;
	unsigned char *string = struri(cached->uri);
	size_t length = strlen(string);
	struct dom_string uri = INIT_DOM_STRING(string, length);

	convert_table = get_convert_table(head, document->options.cp,
					  document->options.assume_cp,
					  &document->cp,
					  &document->cp_status,
					  document->options.hard_assume);

	init_dom_renderer(&renderer, document, buffer, convert_table);

	document->color.background = document->options.default_style.color.background;
#ifdef CONFIG_UTF8
	document->options.utf8 = is_cp_utf8(document->options.cp);
#endif /* CONFIG_UTF8 */

	if (document->options.plain)
		parser_type = SGML_PARSER_STREAM;
	else
		parser_type = SGML_PARSER_TREE;

	get_doctype(&renderer, cached);

	parser = init_sgml_parser(parser_type, renderer.doctype, &uri, 0);
	if (!parser) return;

	if (document->options.plain) {
		add_dom_stack_context(&parser->stack, &renderer,
				      &dom_source_renderer_context_info);

	} else if (renderer.doctype == SGML_DOCTYPE_RSS) {
		add_dom_stack_context(&parser->stack, &renderer,
				      &dom_rss_renderer_context_info);
		add_dom_config_normalizer(&parser->stack, &config, RSS_CONFIG_FLAGS);
	}

	/* FIXME: When rendering this way we don't really care about the code.
	 * However, it will be useful when we will be able to also
	 * incrementally parse new data. This will require the parser to live
	 * during the fetching of data. */
	parse_sgml(parser, buffer->source, buffer->length, 1);
	if (parser->root) {
		assert(parser->stack.depth == 1);

		get_dom_stack_top(&parser->stack)->immutable = 0;
		/* For SGML_PARSER_STREAM this will free the DOM
		 * root node. */
		pop_dom_node(&parser->stack);
	}

	done_dom_renderer(&renderer);
	done_sgml_parser(parser);
}
