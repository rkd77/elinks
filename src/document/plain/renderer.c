/* Plain text document renderer */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
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
#include "util/color.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


struct plain_renderer {
	/* The document being renderered */
	struct document *document;

	/* The data and data length of the defragmented cache entry */
	unsigned char *source;
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
};

#define realloc_document_links(doc, size) \
	ALIGN_LINK(&(doc)->links, (doc)->nlinks, size)

static struct screen_char *
realloc_line(struct document *document, int x, int y)
{
	struct line *line = realloc_lines(document, y);

	if (!line) return NULL;

	if (x != line->length) {
		if (!ALIGN_LINE(&line->chars, line->length, x))
			return NULL;

		line->length = x;
	}

	return line->chars;
}

static inline struct link *
add_document_link(struct document *document, unsigned char *uri, int length,
		  int x, int y)
{
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
check_link_word(struct document *document, unsigned char *uri, int length,
		int x, int y)
{
	struct uri test;
	unsigned char *where = NULL;
	unsigned char *mailto = memchr(uri, '@', length);
	int keep = uri[length];
	struct link *new_link;

	assert(document);
	if_assert_failed return NULL;

	uri[length] = 0;

	if (mailto && mailto > uri && mailto - uri < length - 1) {
		where = straconcat("mailto:", uri, (unsigned char *) NULL);

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
get_uri_length(unsigned char *line, int length)
{
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
		    unsigned char *line, int line_pos, int width,
		    int expanded, struct screen_char *pos, int cells)
{
	struct document *document = renderer->document;
	unsigned char *start = &line[line_pos];
	int len = get_uri_length(start, width - line_pos);
	int screen_column = cells + expanded;
	struct link *new_link;
	int link_end = line_pos + len;
	unsigned char saved_char;
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

static void
decode_esc_color(unsigned char *text, int *line_pos, int width,
		 struct screen_char *template_, enum color_mode mode,
		 int *was_reversed)
{
	struct screen_char ch;
	struct color_pair color;
	char *buf, *tail, *begin, *end;
	int k, foreground, background, f1, b1; /* , intensity; */

	++(*line_pos);
	buf = (char *)&text[*line_pos];

	if (*buf != '[') return;
	++buf;
	++(*line_pos);
	
	k = strspn(buf, "0123456789;");
	*line_pos += k;
	if (!k || buf[k] != 'm')  return;
	
	end = buf + k;
	begin = tail = buf;

	get_screen_char_color(template_, &color, 0, mode);
	set_term_color(&ch, &color, 0, COLOR_MODE_16);
	b1 = background = (ch.c.color[0] >> 4) & 7;
	f1 = foreground = ch.c.color[0] & 15;
	
	while (tail < end) {
		unsigned char kod = (unsigned char)strtol(begin, &tail, 10);

		begin = tail + 1;
		switch (kod) {
		case 0:
			background = 0;
			foreground = 7;
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
		default:
			break;
		}
	}
	color.background = get_term_color16(background);
	color.foreground = get_term_color16(foreground);
	set_term_color(template_, &color, 0, mode);
}

static inline int
add_document_line(struct plain_renderer *renderer,
		  unsigned char *line, int line_width)
{
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
		unsigned char line_char = line[line_pos];
		int charlen = 1;
		int cell = 1;
#ifdef CONFIG_UTF8
		unicode_val_T data;

		if (utf8) {
			unsigned char *line_char2 = &line[line_pos];
			charlen = utf8charlen(&line_char);
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
		unsigned char line_char = line[line_pos];
		unsigned char next_char, prev_char;
		int charlen = 1;
		int cell = 1;
#ifdef CONFIG_UTF8
		unicode_val_T data = UCS_NO_CHAR;

		if (utf8) {
			unsigned char *line_char2 = &line[line_pos];
			charlen = utf8charlen(&line_char);
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
			decode_esc_color(line, &line_pos, width,
					 &saved_renderer_template,
					 doc_opts->color_mode, &was_reversed);
			*template_ = saved_renderer_template;
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
	}
	mem_free(line);

	realloc_line(document, pos - startpos, lineno);

	return width + expanded;
}

static void
init_template(struct screen_char *template_, struct document_options *options)
{
	get_screen_char_template(template_, options, options->default_style);
}

static struct node *
add_node(struct plain_renderer *renderer, int x, int width, int height)
{
	struct node *node = mem_alloc(sizeof(*node));

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
	unsigned char *source = renderer->source;
	int length = renderer->length;
	int was_empty_line = 0;
	int was_wrapped = 0;
#ifdef CONFIG_UTF8
	int utf8 = is_cp_utf8(renderer->document->cp);
#endif
	for (; length > 0; renderer->lineno++) {
		unsigned char *xsource;
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

			if (isspace(source[width])) {
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
				unsigned char *text = &source[width];
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

void
render_plain_document(struct cache_entry *cached, struct document *document,
		      struct string *buffer)
{
	struct conv_table *convert_table;
	unsigned char *head = empty_string_or_(cached->head);
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
	renderer.max_width = document->options.wrap ? document->options.box.width
						    : INT_MAX;

	document->color.background = document->options.default_style.color.background;
	document->width = 0;
#ifdef CONFIG_UTF8
	document->options.utf8 = is_cp_utf8(document->options.cp);
#endif /* CONFIG_UTF8 */

	/* Setup the style */
	init_template(&renderer.template_, &document->options);

	add_document_lines(&renderer);
}
