/* SGML node handling */
/* $Id: docbook.c,v 1.1.2.24 2004/02/29 02:47:30 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/dom/navigator.h"
#include "document/dom/node.h"
#include "document/sgml/docbook/info.h"
#include "document/sgml/parser.h"
#include "document/sgml/scanner.h"
#include "document/sgml/sgml.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


#define DOCBOOK_NODE_INFO(node, name, id)	SGML_NODE_INFO(DOCBOOK, node, name, id)
#define DOCBOOK_NODE_INF2(node, name, str, id)	SGML_NODE_INF2(DOCBOOK, node, name, str, id)

static struct sgml_node_info docbook_attributes[DOCBOOK_ATTRIBUTES] = {
	SGML_NODE_HEAD(DOCBOOK, ATTRIBUTE),

#include "document/sgml/docbook/attribute.inc"
};

static struct sgml_node_info docbook_elements[DOCBOOK_ELEMENTS] = {
	SGML_NODE_HEAD(DOCBOOK, ELEMENT),

#include "document/sgml/docbook/element.inc"
};


static struct dom_node *
add_docbook_element_end_node(struct dom_navigator *navigator, struct dom_node *node, void *data)
{
	struct sgml_parser *parser = navigator->data;
	struct dom_node *parent;
	struct scanner_token *token;

	assert(navigator && parser && node);
	assert(dom_navigator_has_parents(navigator));

	if (!(parser->flags & SGML_PARSER_ADD_ELEMENT_ENDS))
		return NULL;

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


struct sgml_info sgml_docbook_info = {
	docbook_attributes,
	docbook_elements,
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ add_docbook_element_end_node,
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
