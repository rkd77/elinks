#ifndef EL__DOCUMENT_DOM_UTIL_H
#define EL__DOCUMENT_DOM_UTIL_H

/* This header is meant to be used only amongst the DOM renderers. */

#include <sys/types.h> /* FreeBSD needs this before regex.h */
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

#include "dom/sgml/sgml.h"
#include "intl/charsets.h"
#include "terminal/draw.h"


struct document;
struct uri;
struct dom_node;
struct dom_node_list;
struct dom_string;

struct dom_renderer {
	enum sgml_document_type doctype;
	struct document *document;

	struct conv_table *convert_table;
	enum convert_string_mode convert_mode;

	struct uri *base_uri;

	unsigned char *source;
	unsigned char *end;

	unsigned char *position;
	int canvas_x, canvas_y;

#ifdef HAVE_REGEX_H
	regex_t url_regex;
	unsigned int find_url:1;
#endif
	struct screen_char styles[DOM_NODES];

	/* RSS renderer variables */
	struct dom_node *channel;
	struct dom_node_list *items;
	struct dom_node *item;
	struct dom_node *node;
	struct dom_string text;
};

#define X(renderer)		((renderer)->canvas_x)
#define Y(renderer)		((renderer)->canvas_y)


void init_template(struct screen_char *template,
		   struct document_options *options,
                   color_T background, color_T foreground,
		   enum screen_char_attr attr);
void render_dom_text(struct dom_renderer *renderer, struct screen_char *template,
                     unsigned char *string, int length);
struct link *add_dom_link(struct dom_renderer *renderer, unsigned char *string,
                          int length, unsigned char *uristring, int urilength);

#endif
