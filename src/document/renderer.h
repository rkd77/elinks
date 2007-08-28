#ifndef EL__DOCUMENT_RENDERER_H
#define EL__DOCUMENT_RENDERER_H

#include "document/document.h"

struct conv_table;
struct document_options;
struct document_view;
struct session;
struct view_state;
struct screen_char;


enum text_style_format {
	AT_BOLD = 1,
	AT_ITALIC = 2,
	AT_UNDERLINE = 4,
	AT_FIXED = 8,
	AT_GRAPHICS = 16,
	AT_PREFORMATTED = 32,
};

struct text_style {
	enum text_style_format attr;
	color_T fg;
	color_T bg;
};

void get_screen_char_template(struct screen_char *template, struct document_options *options, struct text_style style);


void render_document(struct view_state *, struct document_view *, struct document_options *);
void render_document_frames(struct session *ses, int no_cache);
struct conv_table *get_convert_table(unsigned char *head, int to_cp, int default_cp, int *from_cp, enum cp_status *cp_status, int ignore_server_cp);
void sort_links(struct document *document);

#endif
