
#ifndef EL__DOCUMENT_HTML_DOMPARSER_DOMPARSER_H
#define EL__DOCUMENT_HTML_DOMPARSER_DOMPARSER_H

/* This is internal header for the Domparser engine. It should not be included
 * from anywhere outside, ideally. */

struct dom_node;
struct dom_string;
struct html_context;
struct string;


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
	struct string *title;
};

#define domctx(html_context) ((struct domparser_context *) html_context->data)


/* Interface for other parts of HTML DOM */

void apply_focusable(struct html_context *html_context, struct dom_node *node);

/* These should really belong to /src/dom/... */
struct dom_string *get_dom_element_attr(struct dom_node *elem, int type);
unsigned char *get_dom_element_attr_uri(struct dom_node *elem, int type, struct html_context *html_context);

#endif
