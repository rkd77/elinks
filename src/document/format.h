#ifndef EL__DOCUMENT_FORMAT_H
#define EL__DOCUMENT_FORMAT_H

#include "util/color.h"

struct document_options;
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

#endif

