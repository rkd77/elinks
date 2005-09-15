/* $Id: renderer.h,v 1.70.4.1 2005/01/29 01:49:00 jonas Exp $ */

#ifndef EL__DOCUMENT_HTML_RENDERER_H
#define EL__DOCUMENT_HTML_RENDERER_H

#include "document/document.h"

struct box;
struct cache_entry;
struct string;


void render_html_document(struct cache_entry *cached, struct document *document, struct string *buffer);

/* Interface with tables.c */

struct part {
	struct document *document;

	unsigned char *spaces;
	int spaces_len;

	struct box box;

	int max_width;
	int xa;
	int cx, cy;
	int link_num;
};

void expand_lines(struct part *part, int x, int y, int lines, color_t bgcolor);
void check_html_form_hierarchy(struct part *part);

void draw_frame_hchars(struct part *, int, int, int, unsigned char data, color_t bgcolor, color_t fgcolor);
void draw_frame_vchars(struct part *, int, int, int, unsigned char data, color_t bgcolor, color_t fgcolor);

void free_table_cache(void);
void process_hidden_link(struct part *);

struct part *format_html_part(unsigned char *, unsigned char *, int, int, int, struct document *, int, int, unsigned char *, int);

int find_tag(struct document *document, unsigned char *name, int namelen);

#endif
