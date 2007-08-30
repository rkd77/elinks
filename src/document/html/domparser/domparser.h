
#ifndef EL__DOCUMENT_HTML_MIKUPARSER_MIKUPARSER_H
#define EL__DOCUMENT_HTML_MIKUPARSER_MIKUPARSER_H

/* This is internal header for the Domparser engine. It should not be included
 * from anywhere outside, ideally. */

struct dom_node;


/* Domparser-specific part of html_element */

struct domparser_element {
	/* Did the element already really begin? */
	int element_began:1;

	/* Corresponding node in the DOM tree */
	struct dom_node *node;
};

#define domelem(html_element) ((struct domparser_element *) html_element->data)


/* Domparser-specific part of html_context */

struct domparser_context {
	struct sgml_parser *parser;
};

#define domctx(html_context) ((struct domparser_context *) html_context->data)

#endif
