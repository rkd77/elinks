/* $Id: stack.h,v 1.5 2004/04/23 23:12:10 pasky Exp $ */

#ifndef EL__DOCUMENT_HTML_PARSER_STACK_H
#define EL__DOCUMENT_HTML_PARSER_STACK_H

#include "document/html/parser.h"

struct html_element *search_html_stack(unsigned char *name);

void html_stack_dup(enum html_element_type type);

void kill_html_stack_item(struct html_element *e);
void kill_html_stack_until(int ls, ...);

/* void dump_html_stack(void); */

#endif
