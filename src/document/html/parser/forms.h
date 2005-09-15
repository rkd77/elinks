/* $Id: forms.h,v 1.9 2005/07/09 19:18:04 miciah Exp $ */

#ifndef EL__DOCUMENT_HTML_PARSER_FORMS_H
#define EL__DOCUMENT_HTML_PARSER_FORMS_H

#include "document/html/parser/parse.h"

struct html_context;

element_handler_T html_button;
element_handler_T html_form;
element_handler_T html_input;
element_handler_T html_select;
element_handler_T html_option;
element_handler_T html_textarea;

int do_html_select(unsigned char *attr, unsigned char *html, unsigned char *eof, unsigned char **end, struct html_context *html_context);
void do_html_textarea(unsigned char *attr, unsigned char *html, unsigned char *eof, unsigned char **end, struct html_context *html_context);

#endif
