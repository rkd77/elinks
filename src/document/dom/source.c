/* DOM-based SGML (HTML) source view renderer (just syntax highlighting :-) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h> /* FreeBSD needs this before regex.h */
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

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
#include "util/string.h"


#define check_dom_node_source(renderer, str, len)	\
	((renderer)->source <= (str) && (str) + (len) <= (renderer)->end)

#define assert_source(renderer, str, len) \
	assertm(check_dom_node_source(renderer, str, len), "renderer[%p : %p] str[%p : %p]", \
		(renderer)->source, (renderer)->end, (str), (str) + (len))


#define URL_REGEX "(file://|((f|ht|nt)tp(s)?|smb)://[[:alnum:]]+([-@:.]?[[:alnum:]])*\\.[[:alpha:]]{2,4}(:[[:digit:]]+)?)(/(%[[:xdigit:]]{2}|[-_~&=;?.a-z0-9])*)*"
#define URL_REGFLAGS (REG_ICASE | REG_EXTENDED)


struct source_renderer {
#ifdef HAVE_REGEX_H
	regex_t url_regex;
	unsigned int find_url:1;
#endif

	/* One style per node type. */
	struct screen_char styles[DOM_NODES];
};


static inline void
render_dom_flush(struct dom_renderer *renderer, unsigned char *string)
{
	struct source_renderer *data = renderer->data;
	struct screen_char *template = &data->styles[DOM_NODE_TEXT];
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
	struct source_renderer *data = renderer->data;
	regex_t *regex = &data->url_regex;
	regmatch_t regmatch;
	unsigned char *string = node->string.string;
	int length = node->string.length;
	struct screen_char *template = &data->styles[node->type];
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
render_dom_node_source(struct dom_stack *stack, struct dom_node *node, void *xxx)
{
	struct dom_renderer *renderer = stack->current->data;
	struct source_renderer *data = renderer->data;

	assert(node && renderer && renderer->document);

#ifdef HAVE_REGEX_H
	if (data->find_url
	    && (node->type == DOM_NODE_TEXT
		|| node->type == DOM_NODE_CDATA_SECTION
		|| node->type == DOM_NODE_COMMENT)) {
		render_dom_node_enhanced_text(renderer, node);
	} else
#endif
		render_dom_node_text(renderer, &data->styles[node->type], node);

	return DOM_CODE_OK;
}

/* This callback is also used for rendering processing instruction nodes.  */
static enum dom_code
render_dom_element_source(struct dom_stack *stack, struct dom_node *node, void *xxx)
{
	struct dom_renderer *renderer = stack->current->data;
	struct source_renderer *data = renderer->data;

	assert(node && renderer && renderer->document);

	render_dom_node_text(renderer, &data->styles[node->type], node);

	return DOM_CODE_OK;
}

static enum dom_code
render_dom_element_end_source(struct dom_stack *stack, struct dom_node *node, void *xxx)
{
	struct dom_renderer *renderer = stack->current->data;
	struct source_renderer *data = renderer->data;
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

	render_dom_text(renderer, &data->styles[node->type], string, length);

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

static enum dom_code
render_dom_attribute_source(struct dom_stack *stack, struct dom_node *node, void *xxx)
{
	struct dom_renderer *renderer = stack->current->data;
	struct source_renderer *data = renderer->data;
	struct screen_char *template = &data->styles[node->type];

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

static enum dom_code
render_dom_cdata_source(struct dom_stack *stack, struct dom_node *node, void *xxx)
{
	struct dom_renderer *renderer = stack->current->data;
	struct source_renderer *data = renderer->data;
	unsigned char *string = node->string.string;

	assert(node && renderer && renderer->document);

	/* Highlight the 'CDATA' part of <![CDATA[ if it is there. */
	if (check_dom_node_source(renderer, string - 6, 6)) {
		render_dom_flush(renderer, string - 6);
		render_dom_text(renderer, &data->styles[DOM_NODE_ATTRIBUTE], string - 6, 5);
		renderer->position = string - 1;
		assert_source(renderer, renderer->position, 0);
	}

	render_dom_node_text(renderer, &data->styles[node->type], node);

	return DOM_CODE_OK;
}


static enum dom_code
render_dom_document_start(struct dom_stack *stack, struct dom_node *node, void *xxx)
{
	struct dom_renderer *renderer = stack->current->data;
	struct document *document = renderer->document;
	struct source_renderer *data;
	enum dom_node_type type;

	struct css_stylesheet *css = &default_stylesheet;
	{
		static int i_want_struct_module_for_dom;

		if (!i_want_struct_module_for_dom) {
			static const unsigned char default_colors[] =
				"document	{ color: yellow } "
				"element	{ color: lightgreen } "
				"entity-reference { color: red } "
				"proc-instruction { color: red } "
				"attribute	{ color: magenta } "
				"comment	{ color: aqua } "
				"cdata-section	{ color: orange2 } ";

			i_want_struct_module_for_dom = 1;
			/* When someone will get here earlier than at 4am,
			 * this will be done in some init function, perhaps
			 * not overriding the user's default stylesheet. */
			css_parse_stylesheet(css, NULL, default_colors,
					     default_colors + sizeof(default_colors));
		}
	}

	data = renderer->data = mem_calloc(1, sizeof(*data));

	/* Initialize styles for all the DOM node types. */

	for (type = 0; type < DOM_NODES; type++) {
		struct screen_char *template = &data->styles[type];
		struct dom_string *name = get_dom_node_type_name(type);
		struct css_selector *selector = NULL;

		if (name && is_dom_string_set(name))
			selector = find_css_selector(&css->selectors,
						     CST_ELEMENT, CSR_ROOT,
						     name->string, name->length);
		init_template_by_style(template, &document->options,
				       selector ? &selector->properties : NULL);
	}

#ifdef HAVE_REGEX_H
	if (document->options.plain_display_links) {
		if (regcomp(&data->url_regex, URL_REGEX, URL_REGFLAGS)) {
			regfree(&data->url_regex);
		} else {
			data->find_url = 1;
		}
	}
#endif

	return DOM_CODE_OK;
}

static enum dom_code
render_dom_document_end(struct dom_stack *stack, struct dom_node *node, void *xxx)
{
	struct dom_renderer *renderer = stack->current->data;
	struct source_renderer *data = renderer->data;

	/* If there are no non-element nodes after the last element node make
	 * sure that we flush to the end of the cache entry source including
	 * the '>' of the last element tag if it has one. (bug 519) */
	if (check_dom_node_source(renderer, renderer->position, 0)) {
		render_dom_flush(renderer, renderer->end);
	}

#ifdef HAVE_REGEX_H
	if (data->find_url)
		regfree(&data->url_regex);
#endif

	mem_free(data);

	/* It is not necessary to return DOM_CODE_FREE_NODE here.
	 * Because the parser was created with the SGML_PARSER_STREAM
	 * type, the stack has the DOM_STACK_FLAG_FREE_NODES flag and
	 * implicitly frees all nodes popped from it.  */
	return DOM_CODE_OK;
}


struct dom_stack_context_info dom_source_renderer_context_info = {
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
		/* DOM_NODE_DOCUMENT		*/ render_dom_document_start,
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

