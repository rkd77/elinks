
#ifndef EL__DOCUMENT_HTML_PARSER_LINK_H
#define EL__DOCUMENT_HTML_PARSER_LINK_H

#include "document/html/parser/parse.h"

#ifdef __cplusplus
extern "C" {
#endif

struct html_context;

void put_link_line(const char *prefix, char *linkname, char *link, char *target, struct html_context *html_context);

element_handler_T html_a;
element_handler_T html_applet;
element_handler_T html_iframe;
element_handler_T html_img;
element_handler_T html_link;
element_handler_T html_object;
element_handler_T html_source;
element_handler_T html_audio;
element_handler_T html_video;
element_handler_T html_embed;

#ifdef __cplusplus
}
#endif

#endif
