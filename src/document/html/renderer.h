
#ifndef EL__DOCUMENT_HTML_RENDERER_H
#define EL__DOCUMENT_HTML_RENDERER_H

#include "document/document.h"

struct box;
struct cache_entry;
struct html_context;
struct string;


void render_html_document(struct cache_entry *cached, struct document *document, struct string *buffer);


/* Interface with parser.c */

enum html_special_type {
	SP_TAG,
	SP_FORM,
	SP_CONTROL,
	SP_TABLE,
	SP_USED,
	SP_FRAMESET,
	SP_FRAME,
	SP_NOWRAP,
	SP_CACHE_CONTROL,
	SP_CACHE_EXPIRES,
	SP_REFRESH,
	SP_STYLESHEET,
	SP_COLOR_LINK_LINES,
	SP_SCRIPT,
};


/* Interface with tables.c */

/* This holds some context about what we're currently rendering. We only need
 * one of these, until we start dealing with tables, at which point the table
 * code will create short-lived parts to provide context when it calls routines
 * in the renderer or the parser. */
struct part {
	struct document *document;

	unsigned char *spaces;
	int spaces_len;
#ifdef CONFIG_UTF8
	unsigned char *char_width;
#endif


	struct box box;

	int max_width;
	int xa;
	int cx, cy;
	int link_num;
};

void expand_lines(struct html_context *html_context, struct part *part,
                  int x, int y, int lines, color_T bgcolor);
void check_html_form_hierarchy(struct part *part);

void draw_frame_hchars(struct part *, int, int, int, unsigned char data, color_T bgcolor, color_T fgcolor, struct html_context *html_context);
void draw_frame_vchars(struct part *, int, int, int, unsigned char data, color_T bgcolor, color_T fgcolor, struct html_context *html_context);

void free_table_cache(void);

struct part *format_html_part(struct html_context *html_context, unsigned char *, unsigned char *, int, int, int, struct document *, int, int, unsigned char *, int);

#endif
