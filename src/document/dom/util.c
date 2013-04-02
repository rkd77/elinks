/* Utilities for rendering document bits */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h> /* FreeBSD needs this before regex.h */
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif
#include <string.h>

#include "elinks.h"

#include "bookmarks/bookmarks.h"	/* get_bookmark() */
#include "document/css/property.h"
#include "document/docdata.h"
#include "document/document.h"
#include "document/dom/util.h"
#include "document/format.h"
#include "intl/charsets.h"
#include "globhist/globhist.h"		/* get_global_history_item() */
#include "protocol/uri.h"
#include "terminal/draw.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/box.h"


static inline void
init_template(struct screen_char *template, struct document_options *options,
	      enum screen_char_attr attr, color_T foreground, color_T background)
{
	struct text_style style = INIT_TEXT_STYLE(attr, foreground, background);

	get_screen_char_template(template, options, style);
}

void
init_template_by_style(struct screen_char *template, struct document_options *options,
		       LIST_OF(struct css_property) *properties)
{
	struct text_style style = options->default_style;
	struct css_property *property;

	if (properties) {
		/* TODO: Use the CSS appliers. */
		foreach (property, *properties) {
			switch (property->type) {
			case CSS_PT_BACKGROUND_COLOR:
			case CSS_PT_BACKGROUND:
				if (property->value_type == CSS_VT_COLOR)
					style.color.background = property->value.color;
				break;
			case CSS_PT_COLOR:
				style.color.foreground = property->value.color;
				break;
			case CSS_PT_FONT_WEIGHT:
			case CSS_PT_FONT_STYLE:
			case CSS_PT_TEXT_DECORATION:
				style.attr |= property->value.font_attribute.add;
				break;
			case CSS_PT_DISPLAY:
			case CSS_PT_NONE:
			case CSS_PT_LIST_STYLE:
			case CSS_PT_LIST_STYLE_TYPE:
			case CSS_PT_TEXT_ALIGN:
			case CSS_PT_WHITE_SPACE:
			case CSS_PT_LAST:
				break;
			}
		}
	}

	get_screen_char_template(template, options, style);
}


static struct screen_char *
realloc_line(struct document *document, int x, int y)
{
	struct line *line = realloc_lines(document, y);

	if (!line) return NULL;

	if (x > line->length) {
		if (!ALIGN_LINE(&line->chars, line->length, x))
			return NULL;

		for (; line->length < x; line->length++) {
			line->chars[line->length].data = ' ';
		}

		if (x > document->width) document->width = x;
	}

	return line->chars;
}

static struct node *
add_search_node(struct dom_renderer *renderer, int width)
{
	struct node *node = mem_alloc(sizeof(*node));

	if (node) {
		set_box(&node->box, renderer->canvas_x, renderer->canvas_y,
			width, 1);
		add_to_list(renderer->document->nodes, node);
	}

	return node;
}

#define POS(renderer)		(&(renderer)->document->data[Y(renderer)].chars[X(renderer)])
#define WIDTH(renderer, add)	((renderer)->canvas_x + (add))

static void
render_dom_line(struct dom_renderer *renderer, struct screen_char *template,
		unsigned char *string, int length)
{
	struct document *document = renderer->document;
	struct conv_table *convert = renderer->convert_table;
	enum convert_string_mode mode = renderer->convert_mode;
	int x, charlen;
#ifdef CONFIG_UTF8
	int utf8 = document->options.utf8;
	unsigned char *end;
#endif /* CONFIG_UTF8 */


	assert(renderer && template && string && length);

	string = convert_string(convert, string, length, document->options.cp,
	                        mode, &length, NULL, NULL);
	if (!string) return;

	if (!realloc_line(document, WIDTH(renderer, length), Y(renderer))) {
		mem_free(string);
		return;
	}

	add_search_node(renderer, length);

#ifdef CONFIG_UTF8
	end = string + length;
#endif /* CONFIG_UTF8 */
	for (x = 0, charlen = 1; x < length;x += charlen, renderer->canvas_x++) {
		unsigned char *text = &string[x];

		/* This is mostly to be able to break out so the indentation
		 * level won't get to high. */
		switch (*text) {
		case ASCII_TAB:
		{
			int tab_width = 7 - (X(renderer) & 7);
			int width = WIDTH(renderer, length - x + tab_width);

			template->data = ' ';

			if (!realloc_line(document, width, Y(renderer)))
				break;

			/* Only loop over the expanded tab chars and let the
			 * ``main loop'' add the actual tab char. */
			for (; tab_width-- > 0; renderer->canvas_x++)
				copy_screen_chars(POS(renderer), template, 1);
			charlen = 1;
			break;
		}
		default:
#ifdef CONFIG_UTF8
			if (utf8) {
				unicode_val_T data;
				charlen = utf8charlen(text);
				data = utf8_to_unicode(&text, end);

				template->data = (unicode_val_T)data;

				if (unicode_to_cell(data) == 2) {
					copy_screen_chars(POS(renderer),
							template, 1);

					X(renderer)++;
					template->data = UCS_NO_CHAR;
				}

			} else
#endif /* CONFIG_UTF8 */
				template->data = isscreensafe(*text) ? *text:'.';
		}

		copy_screen_chars(POS(renderer), template, 1);
	}
	mem_free(string);
}

static inline unsigned char *
split_dom_line(unsigned char *line, int length, int *linelen)
{
	unsigned char *end = line + length;
	unsigned char *pos;

	/* End of line detection.
	 * We handle \r, \r\n and \n types here. */
	for (pos = line; pos < end; pos++) {
		int step = 0;

		if (pos[step] == ASCII_CR)
			step++;

		if (pos[step] == ASCII_LF)
			step++;

		if (step) {
			*linelen = pos - line;
			return pos + step;
		}
	}

	*linelen = length;
	return NULL;
}

void
render_dom_text(struct dom_renderer *renderer, struct screen_char *template,
		unsigned char *string, int length)
{
	int linelen;

	for (; length > 0; string += linelen, length -= linelen) {
		unsigned char *newline = split_dom_line(string, length, &linelen);

		if (linelen)
			render_dom_line(renderer, template, string, linelen);

		if (newline) {
			renderer->canvas_y++;
			renderer->canvas_x = 0;
			linelen = newline - string;
		}
	}
}

#define realloc_document_links(doc, size) \
	ALIGN_LINK(&(doc)->links, (doc)->nlinks, size)

NONSTATIC_INLINE struct link *
add_dom_link(struct dom_renderer *renderer, unsigned char *string, int length,
	     unsigned char *uristring, int urilength)
{
	struct document *document = renderer->document;
	int x = renderer->canvas_x;
	int y = renderer->canvas_y;
	unsigned char *where;
	struct link *link;
	struct point *point;
	struct screen_char template;
	color_T fgcolor;

	if (!realloc_document_links(document, document->nlinks + 1))
		return NULL;

	link = &document->links[document->nlinks];

	if (!realloc_points(link, length))
		return NULL;

	uristring = convert_string(renderer->convert_table,
				   uristring, urilength, document->options.cp,
	                           CSM_DEFAULT, NULL, NULL, NULL);
	if (!uristring) return NULL;

	where = join_urls(renderer->base_uri, uristring);

	mem_free(uristring);

	if (!where)
		return NULL;
#ifdef CONFIG_GLOBHIST
	else if (get_global_history_item(where))
		fgcolor = document->options.default_color.link;
#endif
#ifdef CONFIG_BOOKMARKS
	else if (get_bookmark(where))
		fgcolor = document->options.default_color.bookmark_link;
#endif
	else
		fgcolor = document->options.default_color.link;

	link->npoints = length;
	link->type = LINK_HYPERTEXT;
	link->where = where;
	link->color.background = document->options.default_style.color.background;
	link->color.foreground = fgcolor;
	link->number = document->nlinks;

	init_template(&template, &document->options,
		      0, link->color.foreground, link->color.background);

	render_dom_text(renderer, &template, string, length);

	for (point = link->points; length > 0; length--, point++, x++) {
		point->x = x;
		point->y = y;
	}

	document->nlinks++;
	document->links_sorted = 0;

	return link;
}
