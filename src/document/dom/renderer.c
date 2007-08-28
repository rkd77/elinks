/* DOM document renderer */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h> /* FreeBSD needs this before regex.h */
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif
#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "document/css/css.h"
#include "document/css/parser.h"
#include "document/css/property.h"
#include "document/css/stylesheet.h"
#include "document/document.h"
#include "document/dom/renderer.h"
#include "document/dom/util.h"
#include "document/dom/rss.h"
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
#include "terminal/draw.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/snprintf.h"
#include "util/string.h"


#define URL_REGEX "(file://|((f|ht|nt)tp(s)?|smb)://[[:alnum:]]+([-@:.]?[[:alnum:]])*\\.[[:alpha:]]{2,4}(:[[:digit:]]+)?)(/(%[[:xdigit:]]{2}|[-_~&=;?.a-z0-9])*)*"
#define URL_REGFLAGS (REG_ICASE | REG_EXTENDED)


/* Checks the user CSS for properties for each DOM node type name */
static inline void
init_dom_renderer(struct dom_renderer *renderer, struct document *document,
		  struct string *buffer, struct conv_table *convert_table)
{
	enum dom_node_type type;
	struct css_stylesheet *css = &default_stylesheet;

	memset(renderer, 0, sizeof(*renderer));

	renderer->document	= document;
	renderer->convert_table = convert_table;
	renderer->convert_mode	= document->options.plain ? CSM_NONE : CSM_DEFAULT;
	renderer->source	= buffer->source;
	renderer->end		= buffer->source + buffer->length;
	renderer->position	= renderer->source;
	renderer->base_uri	= get_uri_reference(document->uri);

#ifdef HAVE_REGEX_H
	if (renderer->document->options.plain_display_links) {
		if (regcomp(&renderer->url_regex, URL_REGEX, URL_REGFLAGS)) {
			regfree(&renderer->url_regex);
		} else {
			renderer->find_url = 1;
		}
	}
#endif

	for (type = 0; type < DOM_NODES; type++) {
		struct screen_char *template = &renderer->styles[type];
		color_T background = document->options.default_bg;
		color_T foreground = document->options.default_fg;
		enum screen_char_attr attr = 0;
		static int i_want_struct_module_for_dom;

		struct dom_string *name = get_dom_node_type_name(type);
		struct css_selector *selector = NULL;

		if (!i_want_struct_module_for_dom) {
			static const unsigned char default_colors[] =
				"document	{ color: yellow } "
				"element	{ color: lightgreen } "
				"entity-reference { color: red } "
				"proc-instruction { color: red } "
				"attribute	{ color: magenta } "
				"comment	{ color: aqua } "
				"cdata-section	{ color: orange2 } ";
			unsigned char *styles = (unsigned char *) default_colors;

			i_want_struct_module_for_dom = 1;
			/* When someone will get here earlier than at 4am,
			 * this will be done in some init function, perhaps
			 * not overriding the user's default stylesheet. */
			css_parse_stylesheet(css, NULL, styles, styles + sizeof(default_colors));
		}

		if (name)
		if (is_dom_string_set(name))
			selector = find_css_selector(&css->selectors,
						     CST_ELEMENT, CSR_ROOT,
						     name->string, name->length);

		if (selector) {
			struct css_property *property;

			foreach (property, selector->properties) {
				switch (property->type) {
				case CSS_PT_BACKGROUND_COLOR:
				case CSS_PT_BACKGROUND:
					if (property->value_type == CSS_VT_COLOR)
						background = property->value.color;
					break;
				case CSS_PT_COLOR:
					foreground = property->value.color;
					break;
				case CSS_PT_FONT_WEIGHT:
					if (property->value.font_attribute.add & AT_BOLD)
						attr |= SCREEN_ATTR_BOLD;
					break;
				case CSS_PT_FONT_STYLE:
					if (property->value.font_attribute.add & AT_UNDERLINE)
						attr |= SCREEN_ATTR_UNDERLINE;

					if (property->value.font_attribute.add & AT_ITALIC)
						attr |= SCREEN_ATTR_ITALIC;
					break;
				case CSS_PT_TEXT_DECORATION:
					if (property->value.font_attribute.add & AT_UNDERLINE)
						attr |= SCREEN_ATTR_UNDERLINE;
					break;
				case CSS_PT_DISPLAY:
				case CSS_PT_NONE:
				case CSS_PT_TEXT_ALIGN:
				case CSS_PT_WHITE_SPACE:
				case CSS_PT_LAST:
					break;
				}
			}
		}

		init_template(template, &document->options, background, foreground, attr);
	}
}

static inline void
done_dom_renderer(struct dom_renderer *renderer)
{
#ifdef HAVE_REGEX_H
	if (renderer->find_url)
		regfree(&renderer->url_regex);
#endif
	done_uri(renderer->base_uri);
}


/* DOM Source Renderer */

#define check_dom_node_source(renderer, str, len)	\
	((renderer)->source <= (str) && (str) + (len) <= (renderer)->end)

#define assert_source(renderer, str, len) \
	assertm(check_dom_node_source(renderer, str, len), "renderer[%p : %p] str[%p : %p]", \
		(renderer)->source, (renderer)->end, (str), (str) + (len))

static inline void
render_dom_flush(struct dom_renderer *renderer, unsigned char *string)
{
	struct screen_char *template = &renderer->styles[DOM_NODE_TEXT];
	int length = string - renderer->position;

	assert_source(renderer, renderer->position, 0);
	assert_source(renderer, string, 0);

	if (length <= 0) return;
	render_dom_text(renderer, template, renderer->position, length);
	renderer->position = string;

	assert_source(renderer, renderer->position, 0);
}

static inline void
render_dom_node_text(struct dom_renderer *renderer, struct screen_char *template,
		     struct dom_node *node)
{
	unsigned char *string = node->string.string;
	int length = node->string.length;

	if (node->type == DOM_NODE_ENTITY_REFERENCE) {
		string -= 1;
		length += 2;
	}

	if (check_dom_node_source(renderer, string, length)) {
		render_dom_flush(renderer, string);
		renderer->position = string + length;
		assert_source(renderer, renderer->position, 0);
	}

	render_dom_text(renderer, template, string, length);
}

#ifdef HAVE_REGEX_H
static inline void
render_dom_node_enhanced_text(struct dom_renderer *renderer, struct dom_node *node)
{
	regex_t *regex = &renderer->url_regex;
	regmatch_t regmatch;
	unsigned char *string = node->string.string;
	int length = node->string.length;
	struct screen_char *template = &renderer->styles[node->type];
	unsigned char *alloc_string;

	if (check_dom_node_source(renderer, string, length)) {
		render_dom_flush(renderer, string);
		renderer->position = string + length;
		assert_source(renderer, renderer->position, 0);
	}

	alloc_string = memacpy(string, length);
	if (alloc_string)
		string = alloc_string;

	while (length > 0 && !regexec(regex, string, 1, &regmatch, 0)) {
		int matchlen = regmatch.rm_eo - regmatch.rm_so;
		int offset = regmatch.rm_so;

		if (!matchlen || offset < 0 || regmatch.rm_eo > length)
			break;

		if (offset > 0)
			render_dom_text(renderer, template, string, offset);

		string += offset;
		length -= offset;

		add_dom_link(renderer, string, matchlen, string, matchlen);

		length -= matchlen;
		string += matchlen;
	}

	if (length > 0)
		render_dom_text(renderer, template, string, length);

	mem_free_if(alloc_string);
}
#endif

static enum dom_code
render_dom_node_source(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_renderer *renderer = stack->current->data;

	assert(node && renderer && renderer->document);

#ifdef HAVE_REGEX_H
	if (renderer->find_url
	    && (node->type == DOM_NODE_TEXT
		|| node->type == DOM_NODE_CDATA_SECTION
		|| node->type == DOM_NODE_COMMENT)) {
		render_dom_node_enhanced_text(renderer, node);
	} else
#endif
		render_dom_node_text(renderer, &renderer->styles[node->type], node);

	return DOM_CODE_OK;
}

/* This callback is also used for rendering processing instruction nodes.  */
static enum dom_code
render_dom_element_source(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_renderer *renderer = stack->current->data;

	assert(node && renderer && renderer->document);

	render_dom_node_text(renderer, &renderer->styles[node->type], node);

	return DOM_CODE_OK;
}

enum dom_code
render_dom_element_end_source(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_renderer *renderer = stack->current->data;
	struct dom_stack_state *state = get_dom_stack_top(stack);
	struct sgml_parser_state *pstate = get_dom_stack_state_data(stack->contexts[0], state);
	struct dom_scanner_token *token = &pstate->end_token;
	unsigned char *string = token->string.string;
	int length = token->string.length;

	assert(node && renderer && renderer->document);

	if (!string || !length)
		return DOM_CODE_OK;

	if (check_dom_node_source(renderer, string, length)) {
		render_dom_flush(renderer, string);
		renderer->position = string + length;
		assert_source(renderer, renderer->position, 0);
	}

	render_dom_text(renderer, &renderer->styles[node->type], string, length);

	return DOM_CODE_OK;
}

static void
set_base_uri(struct dom_renderer *renderer, unsigned char *value, size_t valuelen)
{
	unsigned char *href = memacpy(value, valuelen);
	unsigned char *uristring;
	struct uri *uri;

	if (!href) return;
	uristring = join_urls(renderer->base_uri, href);
	mem_free(href);

	if (!uristring) return;
	uri = get_uri(uristring, 0);
	mem_free(uristring);

	if (!uri) return;

	done_uri(renderer->base_uri);
	renderer->base_uri = uri;
}

enum dom_code
render_dom_attribute_source(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_renderer *renderer = stack->current->data;
	struct screen_char *template = &renderer->styles[node->type];

	assert(node && renderer->document);

	render_dom_node_text(renderer, template, node);

	if (is_dom_string_set(&node->data.attribute.value)) {
		int quoted = node->data.attribute.quoted == 1;
		unsigned char *value = node->data.attribute.value.string - quoted;
		int valuelen = node->data.attribute.value.length + quoted * 2;

		if (check_dom_node_source(renderer, value, 0)) {
			render_dom_flush(renderer, value);
			renderer->position = value + valuelen;
			assert_source(renderer, renderer->position, 0);
		}

		if (node->data.attribute.reference
		    && valuelen - quoted * 2 > 0) {
			int skips;

			/* Need to flush the first quoting delimiter and any
			 * leading whitespace so that the renderers x position
			 * is at the start of the value string. */
			for (skips = 0; skips < valuelen; skips++) {
				if ((quoted && skips == 0)
				    || isspace(value[skips])
				    || value[skips] < ' ')
					continue;

				break;
			}

			if (skips > 0) {
				render_dom_text(renderer, template, value, skips);
				value    += skips;
				valuelen -= skips;
			}

			/* Figure out what should be skipped after the actual
			 * link text. */
			for (skips = 0; skips < valuelen; skips++) {
				if ((quoted && skips == 0)
				    || isspace(value[valuelen - skips - 1])
				    || value[valuelen - skips - 1] < ' ')
					continue;

				break;
			}

			if (renderer->doctype == SGML_DOCTYPE_HTML
			    && node->data.attribute.type == HTML_ATTRIBUTE_HREF
			    && node->parent->data.element.type == HTML_ELEMENT_BASE) {
				set_base_uri(renderer, value, valuelen - skips);
			}

			add_dom_link(renderer, value, valuelen - skips,
				     value, valuelen - skips);

			if (skips > 0) {
				value += valuelen - skips;
				render_dom_text(renderer, template, value, skips);
			}
		} else {
			render_dom_text(renderer, template, value, valuelen);
		}
	}

	return DOM_CODE_OK;
}

enum dom_code
render_dom_cdata_source(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_renderer *renderer = stack->current->data;
	unsigned char *string = node->string.string;

	assert(node && renderer && renderer->document);

	/* Highlight the 'CDATA' part of <![CDATA[ if it is there. */
	if (check_dom_node_source(renderer, string - 6, 6)) {
		render_dom_flush(renderer, string - 6);
		render_dom_text(renderer, &renderer->styles[DOM_NODE_ATTRIBUTE], string - 6, 5);
		renderer->position = string - 1;
		assert_source(renderer, renderer->position, 0);
	}

	render_dom_node_text(renderer, &renderer->styles[node->type], node);

	return DOM_CODE_OK;
}

enum dom_code
render_dom_document_end(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_renderer *renderer = stack->current->data;

	/* If there are no non-element nodes after the last element node make
	 * sure that we flush to the end of the cache entry source including
	 * the '>' of the last element tag if it has one. (bug 519) */
	if (check_dom_node_source(renderer, renderer->position, 0)) {
		render_dom_flush(renderer, renderer->end);
	}

	return DOM_CODE_OK;
}

static struct dom_stack_context_info dom_source_renderer_context_info = {
	/* Object size: */			0,
	/* Push: */
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ render_dom_element_source,
		/* DOM_NODE_ATTRIBUTE		*/ render_dom_attribute_source,
		/* DOM_NODE_TEXT		*/ render_dom_node_source,
		/* DOM_NODE_CDATA_SECTION	*/ render_dom_cdata_source,
		/* DOM_NODE_ENTITY_REFERENCE	*/ render_dom_node_source,
		/* DOM_NODE_ENTITY		*/ render_dom_node_source,
		/* DOM_NODE_PROC_INSTRUCTION	*/ render_dom_element_source,
		/* DOM_NODE_COMMENT		*/ render_dom_node_source,
		/* DOM_NODE_DOCUMENT		*/ NULL,
		/* DOM_NODE_DOCUMENT_TYPE	*/ render_dom_node_source,
		/* DOM_NODE_DOCUMENT_FRAGMENT	*/ render_dom_node_source,
		/* DOM_NODE_NOTATION		*/ render_dom_node_source,
	},
	/* Pop: */
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ render_dom_element_end_source,
		/* DOM_NODE_ATTRIBUTE		*/ NULL,
		/* DOM_NODE_TEXT		*/ NULL,
		/* DOM_NODE_CDATA_SECTION	*/ NULL,
		/* DOM_NODE_ENTITY_REFERENCE	*/ NULL,
		/* DOM_NODE_ENTITY		*/ NULL,
		/* DOM_NODE_PROC_INSTRUCTION	*/ NULL,
		/* DOM_NODE_COMMENT		*/ NULL,
		/* DOM_NODE_DOCUMENT		*/ render_dom_document_end,
		/* DOM_NODE_DOCUMENT_TYPE	*/ NULL,
		/* DOM_NODE_DOCUMENT_FRAGMENT	*/ NULL,
		/* DOM_NODE_NOTATION		*/ NULL,
	}
};


static void
get_doctype(struct dom_renderer *renderer, struct cache_entry *cached)
{
	if (!strcasecmp("application/rss+xml", cached->content_type)) {
		renderer->doctype = SGML_DOCTYPE_RSS;

	} else if (!strcasecmp("application/docbook+xml",
	                       cached->content_type)) {
		renderer->doctype = SGML_DOCTYPE_DOCBOOK;

	} else if (!strcasecmp("application/xbel+xml", cached->content_type)
		   || !strcasecmp("application/x-xbel", cached->content_type)
		   || !strcasecmp("application/xbel", cached->content_type)) {
		renderer->doctype = SGML_DOCTYPE_XBEL;

	} else {
		assertm(!strcasecmp("text/html", cached->content_type)
			|| !strcasecmp("application/xhtml+xml",
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
	struct conv_table *convert_table;
	struct sgml_parser *parser;
 	enum sgml_parser_type parser_type;
	unsigned char *string = struri(cached->uri);
	size_t length = strlen(string);
	struct dom_string uri = INIT_DOM_STRING(string, length);
	enum dom_code code;

	convert_table = get_convert_table(head, document->options.cp,
					  document->options.assume_cp,
					  &document->cp,
					  &document->cp_status,
					  document->options.hard_assume);

	init_dom_renderer(&renderer, document, buffer, convert_table);

	document->bgcolor = document->options.default_bg;
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
		add_dom_config_normalizer(&parser->stack, RSS_CONFIG_FLAGS);
	}

	/* FIXME: When rendering this way we don't really care about the code.
	 * However, it will be useful when we will be able to also
	 * incrementally parse new data. This will require the parser to live
	 * during the fetching of data. */
	code = parse_sgml(parser, buffer->source, buffer->length, 1);
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
