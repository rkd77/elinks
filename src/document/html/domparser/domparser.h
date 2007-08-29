
#ifndef EL__DOCUMENT_HTML_MIKUPARSER_MIKUPARSER_H
#define EL__DOCUMENT_HTML_MIKUPARSER_MIKUPARSER_H

/* This is internal header for the Domparser engine. It should not be included
 * from anywhere outside, ideally. */


/* Domparser-specific part of html_context */

struct domparser_context {
	struct sgml_parser *parser;
};

#define domctx(html_context) ((struct domparser_context *) html_context->data)

#endif
