/* Experimental DOM-based HTML parser */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "document/css/apply.h"
#include "document/css/css.h"
#include "document/css/parser.h"
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


/* Comes from http://www.w3.org/TR/REC-CSS2/sample.html */
static const unsigned char default_style[] =
"	ADDRESS,"
"	BLOCKQUOTE,"
"	BODY, DD, DIV,"
"	DL, DT,"
"	FIELDSET, FORM,"
"	FRAME, FRAMESET,"
"	H1, H2, H3, H4,"
"	H5, H6, IFRAME,"
"	NOFRAMES,"
"	OBJECT, OL, P,"
"	UL, APPLET,"
"	CENTER, DIR,"
"	HR, MENU, PRE   { display: block }"
"	LI              { display: list-item }"
"	HEAD            { display: none }"
"	TABLE           { display: table }"
"	TR              { display: table-row }"
"	THEAD           { display: table-header-group }"
"	TBODY           { display: table-row-group }"
"	TFOOT           { display: table-footer-group }"
"	COL             { display: table-column }"
"	COLGROUP        { display: table-column-group }"
"	TD, TH          { display: table-cell }"
"	CAPTION         { display: table-caption }"
"	TH              { font-weight: bolder; text-align: center }"
"	CAPTION         { text-align: center }"
"	BODY            { padding: 8px; line-height: 1.33 }"
"	H1              { font-size: 2em; margin: .67em 0 }"
"	H2              { font-size: 1.5em; margin: .83em 0 }"
"	H3              { font-size: 1.17em; margin: 1em 0 }"
"	H4, P,"
"	BLOCKQUOTE, UL,"
"	FIELDSET, FORM,"
"	OL, DL, DIR,"
"	MENU            { margin: 1.33em 0 }"
"	H5              { font-size: .83em; line-height: 1.17em; margin: 1.67em 0 }"
"	H6              { font-size: .67em; margin: 2.33em 0 }"
"	H1, H2, H3, H4,"
"	H5, H6, B,"
"	STRONG          { font-weight: bolder }"
"	BLOCKQUOTE      { margin-left: 40px; margin-right: 40px }"
"	I, CITE, EM,"
"	VAR, ADDRESS    { font-style: italic }"
"	PRE, TT, CODE,"
"	KBD, SAMP       { font-family: monospace }"
"	PRE             { white-space: pre }"
"	BIG             { font-size: 1.17em }"
"	SMALL, SUB, SUP { font-size: .83em }"
"	SUB             { vertical-align: sub }"
"	SUP             { vertical-align: super }"
"	S, STRIKE, DEL  { text-decoration: line-through }"
"	HR              { border: 1px inset }"
"	OL, UL, DIR,"
"	MENU, DD        { margin-left: 40px }"
"	OL              { list-style-type: decimal }"
"	OL UL, UL OL,"
"	UL UL, OL OL    { margin-top: 0; margin-bottom: 0 }"
"	U, INS          { text-decoration: underline }"
"	CENTER          { text-align: center }"
/* "	BR:before       { content: \"\A\" }" */

"	/* An example of style for HTML 4.0's ABBR/ACRONYM elements */"

"	ABBR, ACRONYM   { font-variant: small-caps; letter-spacing: 0.1em }"
"	A[href]         { text-decoration: underline }"
"	:focus          { outline: thin dotted invert }";


static struct dom_string *
get_dom_element_attr(struct dom_node *elem, int type)
{
	struct dom_node *attrn;

	attrn = get_dom_node_child(elem, DOM_NODE_ATTRIBUTE, type);
	if (!attrn)
		return NULL;
	return &attrn->data.attribute.value;
}

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

	if (domelem(html_top)->node) {
		struct dom_element_node *elem;

		assert(domelem(html_top)->node->type == DOM_NODE_ELEMENT);
		elem = &domelem(html_top)->node->data.element;

		/* Totally ugly hack. We just need box model. */
		if (elem->type == HTML_ELEMENT_HTML || elem->type == HTML_ELEMENT_BODY)
			html_context->special_f(html_context, SP_DEFAULT_BACKGROUND, format.style.bg);
	}

	if (is_block_element(html_top)) {
		/* Break line before. */
		html_context->line_break_f(html_context);
	}
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
	domelem(html_top)->node = node;

	return DOM_CODE_OK;
}

static enum dom_code
dom_html_pop_element(struct dom_stack *stack, struct dom_node *node, void *xxx)
{
	struct html_context *html_context = stack->current->data;
	struct dom_element_node *elem = &node->data.element;

	/* In theory we should guard for element_begins() here but it would be
	 * useless work. */


	/* See /src/dom/sgml/html/element.inc for the possible values. */
	switch (elem->type) {
		case HTML_ELEMENT_LINK:
#ifdef CONFIG_CSS
		{
			struct dom_string *ds;

			ds = get_dom_element_attr(node, HTML_ATTRIBUTE_REL);
			if (!ds || strlcmp(ds->string, ds->length, "stylesheet", 10))
				break;

			ds = get_dom_element_attr(node, HTML_ATTRIBUTE_HREF);
			if (!ds)
				break;

			import_css_stylesheet(&html_context->css_styles,
					      html_context->base_href, ds->string, ds->length);
		}
#endif
			break;

		case HTML_ELEMENT_STYLE:
#ifdef CONFIG_CSS
		{
			struct dom_node_list **list;
			struct dom_node *child;
			int index;

			if (!html_context->options->css_enable)
				break;

			/* We are interested in anything but attributes ... */
			list = get_dom_node_list_by_type(node, DOM_NODE_CDATA_SECTION);
			if (!list || !*list)
				break;

			foreach_dom_node (*list, child, index) {
				struct dom_string *value;

				switch (child->type) {
				case DOM_NODE_CDATA_SECTION:
				case DOM_NODE_COMMENT:
				case DOM_NODE_TEXT:
					value = get_dom_node_value(child);
					if (!value)
						continue;
					css_parse_stylesheet(&html_context->css_styles,
							     html_context->base_href,
							     value->string,
							     value->string + value->length);
				}
			}
		}
#endif
			break;
	}


	if (is_block_element(html_top)) {
		/* Break line after. */
		html_context->line_break_f(html_context);
	}

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

	if (!html_top->invisible && is_dom_string_set(&node->string)) {
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
		init_sgml_parser(SGML_PARSER_TREE, SGML_DOCTYPE_HTML, &dom_uri, 0);
	if (!parser) {
		done_html_context(html_context);
		return NULL;
	}

	/* Enable it by default, *we* want full CSS powa'. */
	options->css_display_none = 1;

	add_dom_stack_context(&parser->stack, html_context,
			      &dom_html_renderer_context_info);
	add_dom_config_normalizer(&parser->stack, DOM_CONFIG_NORMALIZE_WHITESPACE | DOM_CONFIG_NORMALIZE_CHARACTERS);

	init_string(title);

#ifdef CONFIG_CSS
	css_parse_stylesheet(&html_context->css_styles, uri,
	                     (unsigned char *) default_style,
			     (unsigned char *) default_style + sizeof(default_style));
#endif

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
		pop_dom_node(&parser->stack);
		done_dom_node(parser->root);
	}
}


unsigned char *
get_attr_value(struct html_context *html_context,
               struct html_element *elem, unsigned char *name)
{
	struct dom_node_list **attrs;
	struct dom_node *attr;
	int index;
	int namelen = strlen(name);

	if (!domelem(html_top)->node)
		return NULL;

	attrs = get_dom_node_list_by_type(domelem(html_top)->node, DOM_NODE_ATTRIBUTE);
	if (!attrs || !*attrs)
		return NULL;

	foreach_dom_node (*attrs, attr, index) {
		if (!strlcmp(attr->string.string, attr->string.length, name, namelen)) {
			return dom_string_acpy(&attr->data.attribute.value);
		}
	}

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
