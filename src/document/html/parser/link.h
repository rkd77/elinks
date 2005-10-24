
#ifndef EL__DOCUMENT_HTML_PARSER_LINK_H
#define EL__DOCUMENT_HTML_PARSER_LINK_H

#include "document/html/parser/parse.h"

struct html_context;

void put_link_line(unsigned char *prefix, unsigned char *linkname, unsigned char *link, unsigned char *target, struct html_context *html_context);

element_handler_T html_a;
element_handler_T html_applet;
element_handler_T html_iframe;
element_handler_T html_img;
element_handler_T html_link;
element_handler_T html_object;
element_handler_T html_embed;

#endif
