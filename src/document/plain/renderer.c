/* Plain text document renderer */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>

#include "elinks.h"

#include "bookmarks/bookmarks.h"
#include "cache/cache.h"
#include "config/options.h"
#include "document/docdata.h"
#include "document/document.h"
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
	struct screen_char template;

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
	link->color.background = document->options.default_bg;
	link->color.foreground = document->options.default_link;
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
		where = straconcat("mailto:", uri, NULL);

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
		    int expanded, struct screen_char *pos)
{
	struct document *document = renderer->document;
	unsigned char *start = &line[line_pos];
	int len = get_uri_length(start, width - line_pos);
	int screen_column = line_pos + expanded;
	struct link *new_link;
	int link_end = line_pos + len;
	unsigned char saved_char;
	struct document_options *doc_opts = &document->options;
	struct screen_char template = renderer->template;
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
		new_link->color.foreground = doc_opts->default_vlink;
#endif
#ifdef CONFIG_BOOKMARKS
	else if (get_bookmark(start))
		new_link->color.foreground = doc_opts->default_bookmark_link;
#endif
	else
		new_link->color.foreground = doc_opts->default_link;

	line[link_end] = saved_char;

	new_link->color.background = doc_opts->default_bg;

	set_term_color(&template, &new_link->color,
		       doc_opts->color_flags, doc_opts->color_mode);

	for (i = len; i; i--) {
		template.data = line[line_pos++];
		copy_screen_chars(pos++, &template, 1);
	}

	return len;
}

static inline int
change_colors(struct screen_char *template, unsigned char *text, struct document *document)
{
	unsigned char fg, bg, bold;
	unsigned char *start = text;

#if defined(CONFIG_88_COLORS) || defined(CONFIG_256_COLORS)
	fg = template->color[0];
	bg = template->color[1];
#else
	fg = template->color[0] & 15;
	bg = template->color[0] >> 4;
#endif
	bold = template->attr & SCREEN_ATTR_BOLD;
	for (start = text; *text && *text != 'm'; text++) {
		switch (*text) {
			case ';':
				break;
			case '3':
				text++;
				if (*text >= '0' && *text < '9')
					fg = *text - '0';
				else
					fg = 7;
				break;
			case '4':
				text++;
				if (*text >= '0' && *text < '9')
					bg = *text - '0';
				else
					bg = 0;
				break;
			case '1':
				bold = SCREEN_ATTR_BOLD;
				break;
			case '0':
				bold = 0;
				break;
			default:
				break;
				
		}
	}
	fg |= bold;
	set_term_color16(template, document->options.color_flags, fg, bg);
	return (int)(text - start);

}

static inline int
add_document_line(struct plain_renderer *renderer,
		  unsigned char *line, int line_width)
{
	struct document *document = renderer->document;
	struct screen_char *template = &renderer->template;
	struct screen_char saved_renderer_template = *template;
	struct screen_char *pos, *startpos;
	int lineno = renderer->lineno;
	int expanded = 0;
	int width = line_width;
	int line_pos;

	line = convert_string(renderer->convert_table, line, width,
	                      document->options.cp, CSM_NONE, &width,
	                      NULL, NULL);
	if (!line) return 0;

	/* Now expand tabs */
	for (line_pos = 0; line_pos < width; line_pos++) {
		unsigned char line_char = line[line_pos];

		if (line_char == ASCII_TAB
		    && (line_pos + 1 == width
			|| line[line_pos + 1] != ASCII_BS)) {
			int tab_width = 7 - ((line_pos + expanded) & 7);

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
	}

	assert(expanded >= 0);

	startpos = pos = realloc_line(document, width + expanded, lineno);
	if (!pos) {
		mem_free(line);
		return 0;
	}

	expanded = 0;
	for (line_pos = 0; line_pos < width; line_pos++) {
		unsigned char line_char = line[line_pos];
		unsigned char next_char, prev_char;

		prev_char = line_pos > 0 ? line[line_pos - 1] : '\0';
		next_char = (line_pos + 1 < width) ? line[line_pos + 1]
						   : '\0';

		switch (line_char) {
		case 27:
			if (next_char != '[') goto normal;
			line_pos += 2;
			line_pos += change_colors(&saved_renderer_template, &line[line_pos], document);
			*template = saved_renderer_template;
			break;
		case ASCII_BS:
			if (!(expanded + line_pos)) {
				/* We've backspaced to the start of the line */
				continue;
			}

			if (pos > startpos)
				pos--;  /* Backspace */

			/* Handle x^H_ as _^Hx, but prevent an infinite loop
			 * swapping two underscores. */
			if (next_char == '_'  && prev_char != '_') {
				/* x^H_ becomes _^Hx */
				if (line_pos - 1 >= 0)
					line[line_pos - 1] = next_char;
				if (line_pos + 1 < width)
					line[line_pos + 1] = prev_char;

				/* Go back and reparse the swapped characters */
				if (line_pos - 2 >= 0)
					line_pos -= 2;
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
					template->attr |= (pos - 1)->attr;
				} else {
					/* Default to bold; seems more useful
					 * than underlining the underscore */
					template->attr |= SCREEN_ATTR_BOLD;
				}

			} else if (pos->data == '_') {
				/* Underline _^Hx */

				template->attr |= SCREEN_ATTR_UNDERLINE;

			} else if (pos->data == next_char) {
				/* Embolden x^Hx */

				template->attr |= SCREEN_ATTR_BOLD;
			}

			/* Handle _^Hx^Hx as both bold and underlined */
			if (template->attr)
				template->attr |= pos->attr;
			break;
		case ASCII_TAB:
			/* Do not expand tabs that precede back-spaces; this saves the
			 * back-space code some trouble. */
			if (next_char != ASCII_BS) {
				int tab_width = 7 - ((line_pos + expanded) & 7);

				expanded += tab_width;

				template->data = ' ';
				do
					copy_screen_chars(pos++, template, 1);
				while (tab_width--);

				*template = saved_renderer_template;
				break;
			}
		default:
normal:
			{
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
								  pos);
				}

				if (added_chars) {
					line_pos += added_chars - 1;
					pos += added_chars;
				} else {
					if (!isscreensafe(line_char) && line_char != 27)
						line_char = '.';
					template->data = line_char;
					copy_screen_chars(pos++, template, 1);

					/* Detect copy of nul chars to screen, this
				 	* should not occur. --Zas */
					assert(line_char);
				}

				*template = saved_renderer_template;
			}
		}
	}

	mem_free(line);

	realloc_line(document, pos - startpos, lineno);

	return width + expanded;
}

static void
init_template(struct screen_char *template, struct document_options *options)
{
	color_T background = options->default_bg;
	color_T foreground = options->default_fg;
	struct color_pair colors = INIT_COLOR_PAIR(background, foreground);

	template->attr = 0;
	template->data = ' ';
	set_term_color(template, &colors,
		       options->color_flags, options->color_mode);
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

	for (; length > 0; renderer->lineno++) {
		unsigned char *xsource;
		int width, added, only_spaces = 1, spaces = 0, was_spaces = 0;
		int last_space = 0;
		int tab_spaces = 0;
		int step = 0;
		int doc_width = int_min(renderer->max_width, length);

		/* End of line detection: We handle \r, \r\n and \n types. */
		for (width = 0; width + tab_spaces < doc_width; width++) {
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

	document->bgcolor = document->options.default_bg;
	document->width = 0;

	/* Setup the style */
	init_template(&renderer.template, &document->options);

	add_document_lines(&renderer);
}
