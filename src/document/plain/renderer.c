/* Plain text document renderer */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bookmarks/bookmarks.h"
#include "cache/cache.h"
#include "config/options.h"
#include "document/docdata.h"
#include "document/document.h"
#include "document/format.h"
#include "document/options.h"
#include "document/plain/renderer.h"
#include "document/renderer.h"
#include "globhist/globhist.h"
#include "intl/charsets.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "terminal/color.h"
#include "terminal/draw.h"
#ifdef CONFIG_LIBSIXEL
#include "terminal/sixel.h"
#endif
#include "util/color.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


struct plain_renderer {
	/* The document being renderered */
	struct document *document;

	/* The data and data length of the defragmented cache entry */
	char *source;
	int length;

	/* The convert table that should be used for converting line strings to
	 * the rendered strings. */
	struct conv_table *convert_table;

	/* The default template char data for text */
	struct screen_char template_;

	/* The maximum width any line can have (used for wrapping text) */
	int max_width;

	/* The current line number */
	int lineno;

	/* Are we doing line compression */
	unsigned int compress:1;

#ifdef CONFIG_LIBSIXEL
	unsigned int sixel:1;
#endif
};

#define realloc_document_links(doc, size) \
	ALIGN_LINK(&(doc)->links, (doc)->nlinks, size)

static struct screen_char *
realloc_line(struct document *document, int x, int y)
{
	ELOG
	struct line *line = realloc_lines(document, y);

	if (!line) return NULL;

	if (x != line->length) {
		if (!ALIGN_LINE(&line->ch.chars, line->length, x))
			return NULL;

		line->length = x;
	}

	return line->ch.chars;
}

static inline struct link *
add_document_link(struct document *document, char *uri, int length,
		  int x, int y)
{
	ELOG
	struct link *link;
	struct point *point;

	if (!realloc_document_links(document, document->nlinks + 1))
		return NULL;

	link = &document->links[document->nlinks];

	if (!realloc_points(link, length))
		return NULL;

	link->npoints = length;
	link->type = LINK_HYPERTEXT;
	link->where = uri;
	link->color.background = document->options.default_style.color.background;
	link->color.foreground = document->options.default_color.link;
	link->number = document->nlinks;

	for (point = link->points; length > 0; length--, point++, x++) {
		point->x = x;
		point->y = y;
	}

	document->nlinks++;
	document->links_sorted = 0;
	return link;
}

/* Searches a word to find an email adress or an URI to add as a link. */
static inline struct link *
check_link_word(struct document *document, char *uri, int length,
		int x, int y)
{
	ELOG
	struct uri test;
	char *where = NULL;
	char *mailto = (char *)memchr(uri, '@', length);
	int keep = uri[length];
	struct link *new_link;

	assert(document);
	if_assert_failed return NULL;

	uri[length] = 0;

	if (mailto && mailto > uri && mailto - uri < length - 1) {
		where = straconcat("mailto:", uri, (char *) NULL);

	} else if (parse_uri(&test, uri) == URI_ERRNO_OK
		   && test.protocol != PROTOCOL_UNKNOWN
		   && (test.datalen || test.hostlen)) {
		where = memacpy(uri, length);
	}

	uri[length] = keep;

	if (!where) return NULL;

	/* We need to reparse the URI and normalize it so that the protocol and
	 * host part are converted to lowercase. */
	normalize_uri(NULL, where);

	new_link = add_document_link(document, where, length, x, y);

	if (!new_link) mem_free(where);

	return new_link;
}

#define url_char(c) (		\
		(c) > ' '	\
		&& (c) != '<'	\
		&& (c) != '>'	\
		&& (c) != '('	\
		&& (c) != ')'	\
		&& !isquote(c))

static inline int
get_uri_length(char *line, int length)
{
	ELOG
	int uri_end = 0;

	while (uri_end < length
	       && url_char(line[uri_end]))
		uri_end++;

	for (; uri_end > 0; uri_end--) {
		if (line[uri_end - 1] != '.'
		    && line[uri_end - 1] != ',')
			break;
	}

	return uri_end;
}

static int
print_document_link(struct plain_renderer *renderer, int lineno,
		    char *line, int line_pos, int width,
		    int expanded, struct screen_char *pos, int cells)
{
	ELOG
	struct document *document = renderer->document;
	char *start = &line[line_pos];
	int len = get_uri_length(start, width - line_pos);
	int screen_column = cells + expanded;
	struct link *new_link;
	int link_end = line_pos + len;
	char saved_char;
	struct document_options *doc_opts = &document->options;
	struct screen_char template_ = renderer->template_;
	int i;

	if (!len) return 0;

	new_link = check_link_word(document, start, len, screen_column,
				   lineno);

	if (!new_link) return 0;

	saved_char = line[link_end];
	line[link_end] = '\0';

	if (0)
		; /* Shut up compiler */
#ifdef CONFIG_GLOBHIST
	else if (get_global_history_item(start))
		new_link->color.foreground = doc_opts->default_color.vlink;
#endif
#ifdef CONFIG_BOOKMARKS
	else if (get_bookmark(start))
		new_link->color.foreground = doc_opts->default_color.bookmark_link;
#endif
	else
		new_link->color.foreground = doc_opts->default_color.link;

	line[link_end] = saved_char;

	new_link->color.background = doc_opts->default_style.color.background;

	set_term_color(&template_, &new_link->color,
		       doc_opts->color_flags, doc_opts->color_mode);

	for (i = len; i; i--) {
		template_.data = line[line_pos++];
		copy_screen_chars(pos++, &template_, 1);
	}

	return len;
}

#define RED_COLOR_MASK          0x00FF0000
#define GREEN_COLOR_MASK        0x0000FF00
#define BLUE_COLOR_MASK         0x000000FF

#define RED_COLOR(color)        (((color) & RED_COLOR_MASK)   >> 16)
#define GREEN_COLOR(color)      (((color) & GREEN_COLOR_MASK) >>  8)
#define BLUE_COLOR(color)       (((color) & BLUE_COLOR_MASK)  >>  0)

static void
decode_esc_color(char *text, int *line_pos, int width,
		 struct screen_char *template_, color_mode_T mode,
		 int *was_reversed)
{
	ELOG
	struct screen_char ch;
	struct color_pair color;
	char *buf, *tail, *begin, *end;
	int k, foreground, background, f1, b1; /* , intensity; */

#ifdef CONFIG_256_COLORS
	int foreground256, background256;
	int was_256 = 0;
#endif

	int was_background = 0;
	int was_foreground = 0;

	int was_24 = 0;

	unsigned char back_red = 0, back_green = 0, back_blue = 0;
	unsigned char fore_red = 0, fore_green = 0, fore_blue = 0;

	++(*line_pos);
	buf = (char *)&text[*line_pos];

	if (*buf != '[') return;
	++buf;
	++(*line_pos);
	
	k = strspn(buf, "0123456789;?");
	*line_pos += k;
	if (!k || (buf[k] != 'm' && buf[k] != 'l' && buf[k] != 'h'))  return;
	
	end = buf + k;
	begin = tail = buf;

	get_screen_char_color(template_, &color, 0, mode);

	back_red = RED_COLOR(color.background);
	back_green = GREEN_COLOR(color.background);
	back_blue = BLUE_COLOR(color.background);

	fore_red = RED_COLOR(color.foreground);
	fore_green = GREEN_COLOR(color.foreground);
	fore_blue = BLUE_COLOR(color.foreground);

	set_term_color(&ch, &color, 0, COLOR_MODE_16);
	b1 = background = (ch.c.color[0] >> 4) & 7;
	f1 = foreground = ch.c.color[0] & 15;

#ifdef CONFIG_256_COLORS
	set_term_color(&ch, &color, 0, COLOR_MODE_256);
	foreground256 = ch.c.color[0];
	background256 = ch.c.color[1];
#endif

	while (tail < end) {
		unsigned char kod = (unsigned char)strtol(begin, &tail, 10);

		begin = tail + 1;

		if (was_background) {
			switch (was_background) {
			case 1:
				if (kod == 2) {
					was_background = 2;
					continue;
				}
#ifdef CONFIG_256_COLORS
				if (kod == 5) {
					was_background = 5;
					continue;
				}
#endif
				was_background = 0;
				continue;
			case 2:
				back_red = kod;
				was_background = 3;
				continue;
			case 3:
				back_green = kod;
				was_background = 4;
				continue;
			case 4:
				back_blue = kod;
				was_background = 0;
				was_24 = 1;
				continue;
#ifdef CONFIG_256_COLORS
			case 5:
				background256 = kod;
				was_background = 0;
				was_256 = 1;
				continue;
#endif
			default:
				was_background = 0;
				continue;
			}
		}

		if (was_foreground) {
			switch (was_foreground) {
			case 1:
				if (kod == 2) {
					was_foreground = 2;
					continue;
				}
#ifdef CONFIG_256_COLORS
				if (kod == 5) {
					was_foreground = 5;
					continue;
				}
#endif
				was_foreground = 0;
				continue;
			case 2:
				fore_red = kod;
				was_foreground = 3;
				continue;
			case 3:
				fore_green = kod;
				was_foreground = 4;
				continue;
			case 4:
				fore_blue = kod;
				was_foreground = 0;
				was_24 = 1;
				continue;
#ifdef CONFIG_256_COLORS
			case 5:
				foreground256 = kod;
				was_foreground = 0;
				was_256 = 1;
				continue;
#endif
			default:
				was_foreground = 0;
				continue;
			}
		}

		switch (kod) {
		case 0:
			background = 0;
			foreground = 7;
			back_red = back_green = back_blue = 0;
			fore_red = fore_green = fore_blue = 255;
			break;
		case 7:
			if (*was_reversed == 0) {
				background = f1 & 7;
				foreground = b1;
				*was_reversed = 1;
			}
			break;
		case 27:
			if (*was_reversed == 1) {
				background = f1 & 7;
				foreground = b1;
				*was_reversed = 0;
			}
			break;
		case 30:
		case 31:
		case 32:
		case 33:
		case 34:
		case 35:
		case 36:
		case 37:
			foreground = kod - 30;
			break;

		case 38:
			was_foreground = 1;
			break;

		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
			background = kod - 40;
			break;
		case 48:
			was_background = 1;
			break;

		default:
			break;
		}
	}
#ifdef CONFIG_256_COLORS
	if (was_256) {
		color.background = get_term_color256(background256);
		color.foreground = get_term_color256(foreground256);
	} else
#endif
	{
		color.background = get_term_color16(background);
		color.foreground = get_term_color16(foreground);
	}
	if (was_24) {
		color.background = (back_red << 16) | (back_green << 8) | back_blue;
		color.foreground = (fore_red << 16) | (fore_green << 8) | fore_blue;
	}
	set_term_color(template_, &color, 0, mode);
}

static inline int
add_document_line(struct plain_renderer *renderer,
		  char *line, int line_width)
{
	ELOG
	struct document *document = renderer->document;
	struct screen_char *template_ = &renderer->template_;
	struct screen_char saved_renderer_template = *template_;
	struct screen_char *pos, *startpos;
	struct document_options *doc_opts = &document->options;
	int was_reversed = 0;

#ifdef CONFIG_UTF8
	int utf8 = doc_opts->utf8;
#endif /* CONFIG_UTF8 */
	int cells = 0;
	int lineno = renderer->lineno;
	int expanded = 0;
	int width = line_width;
	int line_pos;

	line = convert_string(renderer->convert_table, line, width,
	                      document->options.cp, CSM_NONE, &width,
	                      NULL, NULL);
	if (!line) return 0;

	/* Now expand tabs */
	for (line_pos = 0; line_pos < width;) {
		unsigned char line_char = (unsigned char)line[line_pos];
		int charlen = 1;
		int cell = 1;
#ifdef CONFIG_UTF8
		unicode_val_T data;

		if (utf8) {
			char *line_char2 = &line[line_pos];
			charlen = utf8charlen((char *)&line_char);
			data = utf8_to_unicode(&line_char2, &line[width]);

			if (data == UCS_NO_CHAR) {
				line_pos += charlen;
				continue;
			}

			cell = unicode_to_cell(data);
		}
#endif /* CONFIG_UTF8 */

		if (line_char == ASCII_TAB
		    && (line_pos + charlen == width
		      	|| line[line_pos + charlen] != ASCII_BS)) {
		  	int tab_width = 7 - ((cells + expanded) & 7);

			expanded += tab_width;
		} else if (line_char == ASCII_BS) {
#if 0
		This does not work: Suppose we have seventeen spaces
		followed by a back-space; that will call for sixteen
		bytes of memory, but we will print seventeen spaces
		before we hit the back-space -- overflow!

			/* Don't count the character
			 * that the back-space character will delete */
			if (expanded + line_pos)
				expanded--;
#endif
#if 0
			/* Don't count the back-space character */
			if (expanded > 0)
				expanded--;
#endif
		}
		line_pos += charlen;
		cells += cell;
	}

	assert(expanded >= 0);

	startpos = pos = realloc_line(document, width + expanded, lineno);
	if (!pos) {
		mem_free(line);
		return 0;
	}

	cells = 0;
	expanded = 0;
	for (line_pos = 0; line_pos < width;) {
		unsigned char line_char = (unsigned char)line[line_pos];
		char next_char, prev_char;
		int charlen = 1;
		int cell = 1;
#ifdef CONFIG_UTF8
		unicode_val_T data = UCS_NO_CHAR;

		if (utf8) {
			char *line_char2 = &line[line_pos];
			charlen = utf8charlen((char *)&line_char);
			data = utf8_to_unicode(&line_char2, &line[width]);

			if (data == UCS_NO_CHAR) {
				line_pos += charlen;
				continue;
			}

			cell = unicode_to_cell(data);
		}
#endif /* CONFIG_UTF8 */

		prev_char = line_pos > 0 ? line[line_pos - 1] : '\0';
		next_char = (line_pos + charlen < width) ?
				line[line_pos + charlen] : '\0';

		/* Do not expand tabs that precede back-spaces; this saves the
		 * back-space code some trouble. */
		if (line_char == ASCII_TAB && next_char != ASCII_BS) {
			int tab_width = 7 - ((cells + expanded) & 7);

			expanded += tab_width;

			template_->data = ' ';
			do
				copy_screen_chars(pos++, template_, 1);
			while (tab_width--);

			*template_ = saved_renderer_template;

		} else if (line_char == ASCII_BS) {
			if (!(expanded + cells)) {
				/* We've backspaced to the start of the line */
				goto next;
			}
			if (pos > startpos)
				pos--;  /* Backspace */

			/* Handle x^H_ as _^Hx, but prevent an infinite loop
			 * swapping two underscores. */
			if (next_char == '_'  && prev_char != '_') {
				/* x^H_ becomes _^Hx */
				if (line_pos - 1 >= 0)
					line[line_pos - 1] = next_char;
				if (line_pos + charlen < width)
					line[line_pos + charlen] = prev_char;

				/* Go back and reparse the swapped characters */
				if (line_pos - 2 >= 0) {
					cells--;
					line_pos--;
				}
				continue;
			}

			if ((expanded + line_pos) - 2 >= 0) {
				/* Don't count the backspace character or the
				 * deleted character when returning the line's
				 * width or when expanding tabs. */
				expanded -= 2;
			}

			if (pos->data == '_' && next_char == '_') {
				/* Is _^H_ an underlined underscore
				 * or an emboldened underscore? */

				if (expanded + line_pos >= 0
				    && pos - 1 >= startpos
				    && (pos - 1)->attr) {
					/* There is some preceding text,
					 * and it has an attribute; copy it */
					template_->attr |= (pos - 1)->attr;
				} else {
					/* Default to bold; seems more useful
					 * than underlining the underscore */
					template_->attr |= SCREEN_ATTR_BOLD;
				}

			} else if (pos->data == '_') {
				/* Underline _^Hx */

				template_->attr |= SCREEN_ATTR_UNDERLINE;

			} else if (pos->data == next_char) {
				/* Embolden x^Hx */

				template_->attr |= SCREEN_ATTR_BOLD;
			}

			/* Handle _^Hx^Hx as both bold and underlined */
			if (template_->attr)
				template_->attr |= pos->attr;
		} else if (line_char == 27) {
#ifdef CONFIG_LIBSIXEL
			if (renderer->sixel && line_pos + 1 < width && line[line_pos + 1] == 'P') { // && line_pos + 2 < width && line[line_pos + 2] == 'q') {
				while (1) {
					char *end = (char *)memchr(line + line_pos + 1, 27, width - line_pos - 1);

					if (end == NULL) {
						break;
					}
					if (end[1] == '\\') {
						int how_many = add_image_to_document(document, line + line_pos, end + 2 - line - line_pos, lineno, NULL) + 1;

						realloc_line(document, pos - startpos, lineno);

						for (int i = 0; i < how_many; i++) {
							realloc_line(document, 0, lineno + i);
						}
						renderer->lineno += how_many;
						lineno += how_many;
						line_pos = end + 2 - line;
						startpos = pos = realloc_line(document, width, lineno);
						goto zero;
					} else {
						line_pos = end - line;
					}
				}
			} else
#endif
			{
				decode_esc_color(line, &line_pos, width,
					 &saved_renderer_template,
					 doc_opts->color_mode, &was_reversed);
				*template_ = saved_renderer_template;
			}
		} else {
			int added_chars = 0;

			if (document->options.plain_display_links
			    && isalpha(line_char) && isalpha(next_char)) {
				/* We only want to check for a URI if there are
				 * at least two consecutive alphabetic
				 * characters, or if we are at the very start of
				 * the line.  It improves performance a bit.
				 * --Zas */
				added_chars = print_document_link(renderer,
								  lineno, line,
								  line_pos,
								  width,
								  expanded,
								  pos, cells);
			}

			if (added_chars) {
				line_pos += added_chars - 1;
				cells += added_chars - 1;
				pos += added_chars;
			} else {
#ifdef CONFIG_UTF8
				if (utf8) {
					if (data == UCS_NO_CHAR) {
						line_pos += charlen;
						continue;
					}

					template_->data = (unicode_val_T)data;
					copy_screen_chars(pos++, template_, 1);

					if (cell == 2) {
						template_->data = UCS_NO_CHAR;
						copy_screen_chars(pos++,
								  template_, 1);
					}
				} else
#endif /* CONFIG_UTF8 */
				{
					if (!isscreensafe(line_char))
						line_char = '.';
					template_->data = line_char;
					copy_screen_chars(pos++, template_, 1);

					/* Detect copy of nul chars to screen,
					 * this should not occur. --Zas */
					assert(line_char);
				}
			}

			*template_ = saved_renderer_template;
		}
next:
		line_pos += charlen;
		cells += cell;
#ifdef CONFIG_LIBSIXEL
zero:
	;
#endif
	}
	mem_free(line);

	realloc_line(document, pos - startpos, lineno);

	return width + expanded;
}

static void
init_template(struct screen_char *template_, struct document_options *options)
{
	ELOG
	get_screen_char_template(template_, options, options->default_style);
}

static struct node *
add_node(struct plain_renderer *renderer, int x, int width, int height)
{
	ELOG
	struct node *node = (struct node *)mem_alloc(sizeof(*node));

	if (node) {
		struct document *document = renderer->document;

		set_box(&node->box, x, renderer->lineno, width, height);

		int_lower_bound(&document->width, width);
		int_lower_bound(&document->height, height);

		add_to_list(document->nodes, node);
	}

	return node;
}

static void
add_document_lines(struct plain_renderer *renderer)
{
	ELOG
	char *source = renderer->source;
	int length = renderer->length;
	int was_empty_line = 0;
	int was_wrapped = 0;
#ifdef CONFIG_UTF8
	int utf8 = is_cp_utf8(renderer->document->cp);
#endif
	for (; length > 0; renderer->lineno++) {
		char *xsource;
		int width, added, only_spaces = 1, spaces = 0, was_spaces = 0;
		int last_space = 0;
		int tab_spaces = 0;
		int step = 0;
		int cells = 0;

		/* End of line detection: We handle \r, \r\n and \n types. */
		for (width = 0; (width < length) &&
				(cells < renderer->max_width);) {
			if (source[width] == ASCII_CR)
				step++;
			if (source[width + step] == ASCII_LF)
				step++;
			if (step) break;

			if (isspace((unsigned char)source[width])) {
				last_space = width;
				if (only_spaces)
					spaces++;
				else
					was_spaces++;
				if (source[width] == '\t')
					tab_spaces += 7 - ((width + tab_spaces) % 8);
			} else {
				only_spaces = 0;
				was_spaces = 0;
			}
#ifdef CONFIG_UTF8
			if (utf8) {
				char *text = &source[width];
				unicode_val_T data = utf8_to_unicode(&text,
							&source[length]);

				if (data == UCS_NO_CHAR) return;

				cells += unicode_to_cell(data);
				width += utf8charlen(&source[width]);
			} else
#endif /* CONFIG_UTF8 */
			{
				cells++;
				width++;
			}
		}

		if (only_spaces && step) {
			if (was_wrapped || (renderer->compress && was_empty_line)) {
				/* Successive empty lines will appear as one. */
				length -= step + spaces;
				source += step + spaces;
				renderer->lineno--;
				assert(renderer->lineno >= 0);
				continue;
			}
			was_empty_line = 1;

			/* No need to keep whitespaces on an empty line. */
			source += spaces;
			length -= spaces;
			width -= spaces;

		} else {
			was_empty_line = 0;
			was_wrapped = !step;

			if (was_spaces && step) {
				/* Drop trailing whitespaces. */
				width -= was_spaces;
				step += was_spaces;
			}

			if (!step && (width < length) && last_space) {
				width = last_space;
				step = 1;
			}
		}

		assert(width >= 0);

		/* We will touch the supplied source, so better replicate it. */
		xsource = memacpy(source, width);
		if (!xsource) continue;

		added = add_document_line(renderer, xsource, width);
		mem_free(xsource);

		if (added) {
			/* Add (search) nodes on a line by line basis */
			add_node(renderer, 0, added, 1);
		}

		/* Skip end of line chars too. */
		width += step;
		length -= width;
		source += width;
	}

	assert(!length);
}

static void
fixup_tables(struct plain_renderer *renderer)
{
	ELOG
	int y;

	for (y = 0; y < renderer->lineno; y++) {
		int x;
		struct line *prev_line = y > 0 ? &renderer->document->data[y - 1] : NULL;
		struct line *line = &renderer->document->data[y];
		struct line *next_line = y < renderer->lineno - 1 ? &renderer->document->data[y + 1] : NULL;

		for (x = 0; x < line->length; x++) {
			int dir;
#ifdef CONFIG_UTF8
			unicode_val_T ch = line->ch.chars[x].data;
			unicode_val_T prev_char, next_char, up_char, down_char;
#else
			unsigned char ch = line->ch.chars[x].data;
			unsigned char prev_char, next_char, up_char, down_char;
#endif
			if (ch != '+' && ch != '-' && ch != '|') {
				continue;
			}

			prev_char = x > 0 ? line->ch.chars[x - 1].data : ' ';
			next_char = x < line->length - 1 ? line->ch.chars[x + 1].data : ' ';
			up_char = (prev_line && x < prev_line->length) ? prev_line->ch.chars[x].data : ' ';
			down_char = (next_line && x < next_line->length) ? next_line->ch.chars[x].data : ' ';

			switch (ch) {
			case '+':
				dir = 0;
				if (up_char == '|' || up_char == BORDER_SVLINE) dir |= 1;
				if (next_char == '-' || next_char == BORDER_SHLINE) dir |= 2;
				if (down_char == '|' || down_char == BORDER_SVLINE) dir |= 4;
				if (prev_char == '-' || prev_char == BORDER_SHLINE) dir |= 8;

				switch (dir) {
				case 15:
					line->ch.chars[x].data = BORDER_SCROSS;
					line->ch.chars[x].attr = SCREEN_ATTR_FRAME;
					break;
				case 13:
					line->ch.chars[x].data = BORDER_SLTEE;
					line->ch.chars[x].attr = SCREEN_ATTR_FRAME;
					break;
				case 7:
					line->ch.chars[x].data = BORDER_SRTEE;
					line->ch.chars[x].attr = SCREEN_ATTR_FRAME;
					break;
				case 6:
					line->ch.chars[x].data = BORDER_SULCORNER;
					line->ch.chars[x].attr = SCREEN_ATTR_FRAME;
					break;
				case 12:
					line->ch.chars[x].data = BORDER_SURCORNER;
					line->ch.chars[x].attr = SCREEN_ATTR_FRAME;
					break;
				case 3:
					line->ch.chars[x].data = BORDER_SDLCORNER;
					line->ch.chars[x].attr = SCREEN_ATTR_FRAME;
					break;
				case 9:
					line->ch.chars[x].data = BORDER_SDRCORNER;
					line->ch.chars[x].attr = SCREEN_ATTR_FRAME;
					break;
				case 11:
					line->ch.chars[x].data = BORDER_SUTEE;
					line->ch.chars[x].attr = SCREEN_ATTR_FRAME;
					break;
				case 14:
					line->ch.chars[x].data = BORDER_SDTEE;
					line->ch.chars[x].attr = SCREEN_ATTR_FRAME;
					break;
				case 1:
				case 4:
				case 5:
					line->ch.chars[x].data = BORDER_SVLINE;
					line->ch.chars[x].attr = SCREEN_ATTR_FRAME;
					break;
				case 2:
				case 8:
				case 10:
					line->ch.chars[x].data = BORDER_SHLINE;
					line->ch.chars[x].attr = SCREEN_ATTR_FRAME;
					break;
				default:
					break;
				}
				break;
			case '-':
				if (prev_char == BORDER_SHLINE || prev_char == BORDER_SCROSS || prev_char == '+' || prev_char == '|'
				|| prev_char == BORDER_SULCORNER || prev_char == BORDER_SDLCORNER || prev_char == BORDER_SRTEE
				|| prev_char == BORDER_SUTEE || prev_char == BORDER_SDTEE || next_char == '-') {
					line->ch.chars[x].data = BORDER_SHLINE;
					line->ch.chars[x].attr = SCREEN_ATTR_FRAME;
				}
				break;
			case '|':
				if (up_char == BORDER_SVLINE || up_char == '+' || up_char == '|' || up_char == BORDER_SULCORNER
				|| up_char == BORDER_SURCORNER || up_char == BORDER_SCROSS || up_char == BORDER_SRTEE || up_char == BORDER_SLTEE
				|| up_char == BORDER_SDTEE) {
					if (next_char == '-') {
						line->ch.chars[x].data = BORDER_SRTEE;
					} else if (prev_char == BORDER_SHLINE || prev_char == '-') {
						line->ch.chars[x].data = BORDER_SLTEE;
					} else {
						line->ch.chars[x].data = BORDER_SVLINE;
					}
					line->ch.chars[x].attr = SCREEN_ATTR_FRAME;
				}
				break;
			default:
				continue;
			}
		}
	}
}

void
render_plain_document(struct cache_entry *cached, struct document *document,
		      struct string *buffer)
{
	ELOG
	struct conv_table *convert_table;
	char *head = empty_string_or_(cached->head);
	struct plain_renderer renderer;

	convert_table = get_convert_table(head, document->options.cp,
					  document->options.assume_cp,
					  &document->cp,
					  &document->cp_status,
					  document->options.hard_assume);

	renderer.source = buffer->source;
	renderer.length = buffer->length;

	renderer.document = document;
	renderer.lineno = 0;
	renderer.convert_table = convert_table;
	renderer.compress = document->options.plain_compress_empty_lines;
#ifdef CONFIG_LIBSIXEL
	renderer.sixel = document->options.sixel;
#endif
	renderer.max_width = document->options.wrap ? document->options.document_width
						    : INT_MAX;

	document->color.background = document->options.default_style.color.background;
	document->width = 0;
#ifdef CONFIG_UTF8
	document->options.utf8 = is_cp_utf8(document->options.cp);
#endif /* CONFIG_UTF8 */

	/* Setup the style */
	init_template(&renderer.template_, &document->options);

	add_document_lines(&renderer);

	if (document->options.plain_fixup_tables) {
		fixup_tables(&renderer);
	}
}
