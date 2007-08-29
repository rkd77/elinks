/* Experimental DOM-based HTML parser */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "document/css/apply.h"
#include "document/css/css.h"
#include "document/css/stylesheet.h"
#include "document/html/domparser/domparser.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/options.h"
#include "dom/configuration.h"
#include "dom/scanner.h"
#include "dom/sgml/parser.h"
#include "dom/sgml/html/html.h"
#include "dom/sgml/rss/rss.h"
#include "dom/node.h"
#include "dom/stack.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

#include "document/html/internal.h"


#ifdef CONFIG_CSS
static void
apply_style(struct html_context *html_context)
{
	struct css_selector *selector;

	selector = get_css_selector_for_element(html_context, html_top,
						&html_context->css_styles,
						&html_context->stack);
	if (!selector) return;

	apply_css_selector_style(html_context, html_top, selector);
	done_css_selector(selector);
}
#endif

/* This should be called after we are through all the element attributes but
 * before we hit the actual element contents. */
static void
element_begins(struct html_context *html_context)
{
	assert(!domelem(html_top)->element_began);
	domelem(html_top)->element_began = 1;

#ifdef CONFIG_CSS
	if (html_top->name) /* No styles for the root element */
		apply_style(html_context);
#endif
}

#define element_beginning_guard	\
	if (!domelem(html_top)->element_began) \
		element_begins(html_context)


static enum dom_code
dom_html_push_element(struct dom_stack *stack, struct dom_node *node, void *xxx)
{
	struct html_context *html_context = stack->current->data;

	element_beginning_guard;

	if (!is_dom_string_set(&node->string))
		return DOM_CODE_OK;

	dup_html_element(html_context);
	html_top->data = mem_calloc(1, sizeof(*domelem(html_top)));
	html_top->name = node->string.string;
	html_top->namelen = node->string.length;
	return DOM_CODE_OK;
}

static enum dom_code
dom_html_pop_element(struct dom_stack *stack, struct dom_node *node, void *xxx)
{
	struct html_context *html_context = stack->current->data;

	/* In theory we should guard for element_begins() here but it would be
	 * useless work. */

	done_html_element(html_context, html_top);
	return DOM_CODE_OK;
}

static enum dom_code
dom_html_push_attribute(struct dom_stack *stack, struct dom_node *node, void *xxx)
{
	struct html_context *html_context = stack->current->data;
	struct dom_attribute_node *attr = &node->data.attribute;

	/* See /src/dom/sgml/html/attribute.inc for the possible values. */
	switch (attr->type) {
		case HTML_ATTRIBUTE_CLASS:
			mem_free_set(&format.class, dom_string_acpy(&attr->value));
			break;

		case HTML_ATTRIBUTE_ID:
			mem_free_set(&format.id, dom_string_acpy(&attr->value));
			html_context->special_f(html_context, SP_TAG,
			                        attr->value.string);
			break;
	}
	return DOM_CODE_OK;
}

static enum dom_code
dom_html_pop_text(struct dom_stack *stack, struct dom_node *node, void *xxx)
{
	struct html_context *html_context = stack->current->data;

	element_beginning_guard;

	if (is_dom_string_set(&node->string)) {
		html_context->put_chars_f(html_context,
				node->string.string, node->string.length);
	}
	return DOM_CODE_OK;
}


struct dom_stack_context_info dom_html_renderer_context_info = {
	/* Object size: */			0,
	/* Push: */
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ dom_html_push_element,
		/* DOM_NODE_ATTRIBUTE		*/ dom_html_push_attribute,
		/* DOM_NODE_TEXT		*/ NULL,
		/* DOM_NODE_CDATA_SECTION	*/ NULL,
		/* DOM_NODE_ENTITY_REFERENCE	*/ NULL,
		/* DOM_NODE_ENTITY		*/ NULL,
		/* DOM_NODE_PROC_INSTRUCTION	*/ NULL,
		/* DOM_NODE_COMMENT		*/ NULL,
		/* DOM_NODE_DOCUMENT		*/ NULL,
		/* DOM_NODE_DOCUMENT_TYPE	*/ NULL,
		/* DOM_NODE_DOCUMENT_FRAGMENT	*/ NULL,
		/* DOM_NODE_NOTATION		*/ NULL,
	},
	/* Pop: */
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ dom_html_pop_element,
		/* DOM_NODE_ATTRIBUTE		*/ NULL,
		/* DOM_NODE_TEXT		*/ dom_html_pop_text,
		/* DOM_NODE_CDATA_SECTION	*/ NULL,
		/* DOM_NODE_ENTITY_REFERENCE	*/ NULL,
		/* DOM_NODE_ENTITY		*/ NULL,
		/* DOM_NODE_PROC_INSTRUCTION	*/ NULL,
		/* DOM_NODE_COMMENT		*/ NULL,
		/* DOM_NODE_DOCUMENT		*/ NULL,
		/* DOM_NODE_DOCUMENT_TYPE	*/ NULL,
		/* DOM_NODE_DOCUMENT_FRAGMENT	*/ NULL,
		/* DOM_NODE_NOTATION		*/ NULL,
	}
};


struct html_context *
init_html_parser(struct uri *uri, struct document_options *options,
		 unsigned char *start, unsigned char *end,
		 struct string *head, struct string *title,
		 void (*put_chars)(struct html_context *,
		                   unsigned char *, int),
		 void (*line_break)(struct html_context *),
		 void *(*special)(struct html_context *,
			          enum html_special_type, ...))
{
	struct html_context *html_context;
	struct dom_string dom_uri = INIT_DOM_STRING(struri(uri), strlen(struri(uri)));
	struct sgml_parser *parser;

	html_context = init_html_context(uri, options,
	                                 put_chars, line_break, special);
	if (!html_context) return NULL;
	html_context->data = mem_calloc(1, sizeof(*domctx(html_context)));
	if (!domctx(html_context)) {
		done_html_context(html_context);
		return NULL;
	}

	html_top->data = mem_calloc(1, sizeof(*domelem(html_top)));

	domctx(html_context)->parser = parser =
		init_sgml_parser(SGML_PARSER_STREAM, SGML_DOCTYPE_HTML, &dom_uri, 0);
	if (!parser) {
		done_html_context(html_context);
		return NULL;
	}

	add_dom_stack_context(&parser->stack, html_context,
			      &dom_html_renderer_context_info);
	add_dom_config_normalizer(&parser->stack, DOM_CONFIG_NORMALIZE_WHITESPACE | DOM_CONFIG_NORMALIZE_CHARACTERS);

	init_string(title);

	return html_context;
}

void
done_html_parser(struct html_context *html_context)
{
	done_sgml_parser(domctx(html_context)->parser);
	done_html_context(html_context);
}

void *
init_html_parser_state(struct html_context *html_context, int is_root,
	               int align, int margin, int width)
{
	par_format.align = align;
	par_format.leftmargin = margin;
	par_format.width = width;
	return NULL;
}

void
done_html_parser_state(struct html_context *html_context,
		       void *state)
{
}

void
parse_html(unsigned char *html, unsigned char *eof, struct part *part,
           unsigned char *head, struct html_context *html_context)
{
	struct sgml_parser *parser = domctx(html_context)->parser;

	html_context->part = part;

	parse_sgml(parser, html, eof - html, 1);
	if (parser->root) {
		assert(parser->stack.depth == 1);

		get_dom_stack_top(&parser->stack)->immutable = 0;
		/* For SGML_PARSER_STREAM this will free the DOM
		 * root node. */
		pop_dom_node(&parser->stack);
	}
}


unsigned char *
get_attr_value(struct html_context *html_context,
               struct html_element *elem, unsigned char *name)
{
	return NULL;
}

int
get_image_map(unsigned char *head, unsigned char *pos, unsigned char *eof,
	      struct menu_item **menu, struct memory_list **ml, struct uri *uri,
	      struct document_options *options, unsigned char *target_base,
	      int to, int def, int hdef)
{
	INTERNAL("IMGMAPs not supported for DOM HTML parser");
	return 1;
}
