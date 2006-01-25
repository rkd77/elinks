
#ifndef EL__DOCUMENT_HTML_PARSER_STACK_H
#define EL__DOCUMENT_HTML_PARSER_STACK_H

#include "document/html/parser.h"

struct html_context;

struct html_element *search_html_stack(struct html_context *html_context,
                                       unsigned char *name);

void html_stack_dup(struct html_context *html_context,
                    enum html_element_mortality_type type);

void kill_html_stack_item(struct html_context *html_context,
                          struct html_element *e);
#define pop_html_element(html_context) \
	kill_html_stack_item(html_context, html_top)
void kill_html_stack_until(struct html_context *html_context, int ls, ...);

/* void dump_html_stack(struct html_context *html_context); */

#endif
