
#ifndef EL__DOCUMENT_HTML_PARSER_GENERAL_H
#define EL__DOCUMENT_HTML_PARSER_GENERAL_H

#include "document/html/parser/parse.h"

element_handler_T html_address;
element_handler_T html_base;
element_handler_T html_blockquote;
element_handler_T html_body;
element_handler_T html_bold;
element_handler_T html_br;
element_handler_T html_center;
element_handler_T html_dd;
element_handler_T html_dl;
element_handler_T html_dt;
element_handler_T html_fixed;
element_handler_T html_font;
element_handler_T html_frame;
element_handler_T html_frameset;
element_handler_T html_h1;
element_handler_T html_h2;
element_handler_T html_h3;
element_handler_T html_h4;
element_handler_T html_h5;
element_handler_T html_h6;
element_handler_T html_head;
element_handler_T html_html;
element_handler_T html_html_close;
element_handler_T html_hr;
element_handler_T html_italic;
element_handler_T html_li;
element_handler_T html_linebrk;
element_handler_T html_meta;
element_handler_T html_noframes;
element_handler_T html_noscript;
element_handler_T html_ol;
element_handler_T html_p;
element_handler_T html_pre;
element_handler_T html_quote;
element_handler_T html_quote_close;
element_handler_T html_script;
element_handler_T html_span;
element_handler_T html_style;
element_handler_T html_style_close;
element_handler_T html_subscript;
element_handler_T html_subscript_close;
element_handler_T html_superscript;
element_handler_T html_superscript_close;
element_handler_T html_table;
element_handler_T html_td;
element_handler_T html_th;
element_handler_T html_title;
element_handler_T html_tr;
element_handler_T html_tt;
element_handler_T html_ul;
element_handler_T html_underline;
element_handler_T html_xmp;
element_handler_T html_xmp_close;

void html_apply_canvas_bgcolor(struct html_context *);
void html_handle_body_meta(struct html_context *, unsigned char *, unsigned char *);

#endif
