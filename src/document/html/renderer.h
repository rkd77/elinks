
#ifndef EL__DOCUMENT_HTML_RENDERER_H
#define EL__DOCUMENT_HTML_RENDERER_H

#include "document/document.h"

#ifdef __cplusplus
extern "C" {
#endif

struct el_box;
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
	SP_IFRAME,
	SP_IMAGE
};

typedef unsigned int html_special_type_T;


/* Interface with tables.c */

/* This holds some context about what we're currently rendering. We only need
 * one of these, until we start dealing with tables, at which point the table
 * code will create short-lived parts to provide context when it calls routines
 * in the renderer or the parser. */
struct part {
	struct document *document;

	char *spaces;
	int spaces_len;
#ifdef CONFIG_UTF8
	char *char_width;
#endif


	struct el_box box;

	int max_width;
	int xa;
	int cx, cy;
	int link_num;
	unsigned int begin:1;
};

struct link_state_info {
	char *link;
	char *target;
	char *image;
	struct el_form_control *form;
};

struct renderer_context {
	int last_link_to_move;
	struct tag *last_tag_to_move;
	/* All tags between document->tags and this tag (inclusive) should
	 * be aligned to the next line break, unless some real content follows
	 * the tag. Therefore, this virtual tags list accumulates new tags as
	 * they arrive and empties when some real content is written; if a line
	 * break is inserted in the meanwhile, the tags follow it (ie. imagine
	 * <a name="x"> <p>, then the "x" tag follows the line breaks inserted
	 * by the <p> tag). */
	struct tag *last_tag_for_newline;

	struct link_state_info link_state_info;

	struct conv_table *convert_table;

	/* Used for setting cache info from HTTP-EQUIV meta tags. */
	struct cache_entry *cached;

	int g_ctrl_num;
	int subscript;	/* Count stacked subscripts */
	int supscript;	/* Count stacked supscripts */

	unsigned int empty_format:1;
	unsigned int nobreak:1;
	unsigned int nosearchable:1;
	unsigned int nowrap:1; /* Activated/deactivated by SP_NOWRAP. */
};

extern struct renderer_context renderer_context;

void expand_lines(struct html_context *html_context, struct part *part,
                  int x, int y, int lines, color_T bgcolor);
void check_html_form_hierarchy(struct part *part);

void draw_blockquote_chars(struct part *part, int y, struct html_context *html_context);
void draw_frame_hchars(struct part *, int, int, int, unsigned char data, color_T bgcolor, color_T fgcolor, struct html_context *html_context);
void draw_frame_vchars(struct part *, int, int, int, unsigned char data, color_T bgcolor, color_T fgcolor, struct html_context *html_context);

void free_table_cache(void);

struct part *format_html_part(struct html_context *html_context, char *, char *, int, int, int, struct document *, int, int, char *, int);

int dec2qwerty(int num, char *link_sym, const char *key, int base);
int qwerty2dec(const char *link_sym, const char *key, int base);

void put_chars_conv(struct html_context *html_context, const char *chars, int charslen);
void line_break(struct html_context *html_context);

void *html_special(struct html_context *html_context, html_special_type_T c, ...);


#ifdef __cplusplus
}
#endif

#endif
