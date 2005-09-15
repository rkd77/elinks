/* SGML node handling */
/* $Id: html.c,v 1.3 2004/12/20 00:08:07 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/dom/navigator.h"
#include "document/dom/node.h"
#include "document/sgml/html/html.h"
#include "document/sgml/parser.h"
#include "document/sgml/scanner.h"
#include "document/sgml/sgml.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


#define HTML_NODE_INFO(node, name, id)		SGML_NODE_INFO(HTML, node, name, id)
#define HTML_NODE_INF2(node, name, str, id)	SGML_NODE_INF2(HTML, node, name, str, id)
#undef VERSION

static struct sgml_node_info html_attributes[HTML_ATTRIBUTES] = {
	SGML_NODE_HEAD(HTML, ATTRIBUTE),

#include "document/sgml/html/attribute.inc"
};

static struct sgml_node_info html_elements[HTML_ELEMENTS] = {
	SGML_NODE_HEAD(HTML, ELEMENT),

#include "document/sgml/html/element.inc"
};


static struct dom_node *
add_html_element_end_node(struct dom_navigator *navigator, struct dom_node *node, void *data)
{
	struct sgml_parser *parser = navigator->data;
	struct dom_node *parent;
	struct scanner_token *token;

	assert(navigator && parser && node);
	assert(dom_navigator_has_parents(navigator));

	/* Are we the actual node being popped? */
	if (node != get_dom_navigator_top(navigator)->node)
		return NULL;

	parent = get_dom_navigator_parent(navigator)->node;
	token  = get_scanner_token(&parser->scanner);

	assertm(token, "No token found in callback");
	assertm(token->type == SGML_TOKEN_ELEMENT_END, "Bad token found in callback");

	if (!token->length) return NULL;

	return add_dom_element(parent, token->string, token->length);
}

/* TODO: We need to handle ascending of <br> and "<p>text1<p>text2" using data
 * from sgml_node_info. */
static struct dom_node *
add_html_element_node(struct dom_navigator *navigator, struct dom_node *node, void *data)
{
	struct sgml_parser *parser = navigator->data;

	assert(navigator && node);
	assert(dom_navigator_has_parents(navigator));

	/* TODO: Move to SGML parser main loop and disguise these element ends
	 * in some internal processing instruction. */
	if (parser->flags & SGML_PARSER_ADD_ELEMENT_ENDS)
		get_dom_navigator_top(navigator)->callback = add_html_element_end_node;

	return node;
}


struct sgml_info sgml_html_info = {
	html_attributes,
	html_elements,
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ add_html_element_node,
		/* DOM_NODE_ATTRIBUTE		*/ NULL,
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
	}
};
