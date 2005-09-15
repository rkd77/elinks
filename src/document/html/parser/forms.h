/* $Id: forms.h,v 1.4 2004/12/18 01:42:18 pasky Exp $ */

#ifndef EL__DOCUMENT_HTML_PARSER_FORMS_H
#define EL__DOCUMENT_HTML_PARSER_FORMS_H

struct part;

void html_button(unsigned char *a);
void html_form(unsigned char *a);
void html_input(unsigned char *a);
void html_select(unsigned char *a);
void html_option(unsigned char *a);
void html_textarea(unsigned char *a);

int do_html_select(unsigned char *attr, unsigned char *html, unsigned char *eof, unsigned char **end, struct part *part);
void do_html_textarea(unsigned char *attr, unsigned char *html, unsigned char *eof, unsigned char **end, struct part *part);

#endif
