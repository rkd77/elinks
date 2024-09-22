/* HTML renderer */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

/* Our current implementation of combining characters requires
 * wcwidth().  Therefore the configure script should have disabled
 * CONFIG_COMBINE if wcwidth() doesn't exist.  */
#ifdef CONFIG_COMBINE
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500	/* for wcwidth */
#endif
#endif

#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "document/docdata.h"
#include "document/document.h"
#include "document/html/frames.h"
#include "document/html/iframes.h"
#include "document/html/parser.h"
#include "document/html/parser/parse.h"
#include "document/html/renderer.h"
#include "document/html/tables.h"
#include "document/options.h"
#include "document/refresh.h"
#include "document/renderer.h"
#include "intl/charsets.h"
#include "osdep/types.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "terminal/color.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/hash.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"
#include "util/time.h"
#include "viewer/text/form.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"

/* Unsafe macros */
#include "document/html/internal.h"

/* Types and structs */

enum link_state {
	LINK_STATE_NONE,
	LINK_STATE_NEW,
	LINK_STATE_SAME,
};

typedef unsigned char link_state_T;

struct table_cache_entry_key {
	char *start;
	char *end;
	int align;
	int margin;
	int width;
	int x;
	int link_num;
};

struct table_cache_entry {
	LIST_HEAD_EL(struct table_cache_entry);

	struct table_cache_entry_key key;
	struct part part;
};

/* Max. entries in table cache used for nested tables. */
#define MAX_TABLE_CACHE_ENTRIES 16384

/* Global variables */
static int table_cache_entries;
static struct hash *table_cache;

struct renderer_context renderer_context;

/* Prototypes */
static void put_chars(struct html_context *, const char *, int);

#define X(x_)	(part->box.x + (x_))
#define Y(y_)	(part->box.y + (y_))

#define SPACES_GRANULARITY	0x7F

#define ALIGN_SPACES(x, o, n) mem_align_alloc(x, o, n, SPACES_GRANULARITY)

static inline void
set_screen_char_color(struct screen_char *schar,
		      color_T bgcolor, color_T fgcolor,
		      color_flags_T color_flags,
		      color_mode_T color_mode)
{
	struct color_pair colors = INIT_COLOR_PAIR(bgcolor, fgcolor);

	set_term_color(schar, &colors, color_flags, color_mode);
}

static int
realloc_line(struct html_context *html_context, struct document *document,
             int y, int length)
{
	struct screen_char *pos, *end;
	struct line *line;
	int orig_length;

	if (!realloc_lines(document, y))
		return -1;

	line = &document->data[y];
	orig_length = line->length;

	if (length < orig_length)
		return orig_length;

	if (!ALIGN_LINE(&line->ch.chars, line->length, length + 1))
		return -1;

	/* We cannot rely on the aligned allocation to clear the members for us
	 * since for line splitting we simply trim the length. Question is if
	 * it is better to to clear the line after the splitting or here. */
	end = &line->ch.chars[length];
	end->data = ' ';
	end->attr = 0;
	set_screen_char_color(end, par_elformat.color.background, 0x0,
			      COLOR_ENSURE_CONTRAST, /* for bug 461 */
			      document->options.color_mode);

	for (pos = &line->ch.chars[line->length]; pos < end; pos++) {
		copy_screen_chars(pos, end, 1);
	}

	line->length = length + 1;

	return orig_length;
}

static inline int
realloc_spaces(struct part *part, int length)
{
	if (length < part->spaces_len)
		return 0;

	if (!ALIGN_SPACES(&part->spaces, part->spaces_len, length))
		return -1;
#ifdef CONFIG_UTF8
	if (!ALIGN_SPACES(&part->char_width, part->spaces_len, length))
		return -1;
#endif

	part->spaces_len = length;

	return 0;
}


#define LINE(y_)	part->document->data[Y(y_)]
#define POS(x_, y_)	LINE(y_).ch.chars[X(x_)]
#define LEN(y_)		int_max(LINE(y_).length - part->box.x, 0)


/* When we clear chars we want to preserve and use the background colors
 * already in place else we could end up ``staining'' the background especial
 * when drawing table cells. So make the cleared chars share the colors in
 * place.
 *
 * This function does not update document.comb_x and document.comb_y.
 * That is the caller's responsibility.  */
static inline void
clear_hchars(struct html_context *html_context, int x, int y, int width,
	     struct screen_char *a)
{
	struct part *part;
	struct screen_char *pos, *end;

	assert(html_context);
	if_assert_failed return;

	part = html_context->part;

	assert(part && part->document && width > 0);
	if_assert_failed return;

	if (realloc_line(html_context, part->document, Y(y), X(x) + width - 1) < 0)
		return;

	assert(part->document->data);
	if_assert_failed return;

	pos = &POS(x, y);
	end = pos + width - 1;
	end->data = ' ';
	end->attr = 0;
	end->c = a->c;

	while (pos < end)
		copy_screen_chars(pos++, end, 1);
}

/* TODO: Merge parts with get_format_screen_char(). --jonas */
/* Allocates the required chars on the given line and returns the char at
 * position (x, y) ready to be used as a template char.  */
static inline struct screen_char *
get_frame_char(struct html_context *html_context, struct part *part,
	       int x, int y, unsigned char data,
               color_T bgcolor, color_T fgcolor)
{
	struct screen_char *template_;

	assert(html_context);
	if_assert_failed return NULL;

	assert(part && part->document && x >= 0 && y >= 0);
	if_assert_failed return NULL;

	if (realloc_line(html_context, part->document, Y(y), X(x)) < 0)
		return NULL;

	assert(part->document->data);
	if_assert_failed return NULL;

	template_ = &POS(x, y);
	template_->data = data;
	template_->attr = SCREEN_ATTR_FRAME;
	set_screen_char_color(template_, bgcolor, fgcolor,
			      part->document->options.color_flags,
			      part->document->options.color_mode);

	return template_;
}

static inline struct screen_char *
get_format_screen_char(struct html_context *html_context,
                       link_state_T link_state)
{
	static struct text_style ta_cache = INIT_TEXT_STYLE(-1, 0x0, 0x0);
	static struct screen_char schar_cache;

	if (memcmp(&ta_cache, &elformat.style, sizeof(ta_cache))) {
		copy_struct(&ta_cache, &elformat.style);
		struct text_style final_style = elformat.style;

		get_screen_char_template(&schar_cache, html_context->options, final_style);
	}

	if (!!(schar_cache.attr & SCREEN_ATTR_UNSEARCHABLE)
	    ^ !!renderer_context.nosearchable) {
		schar_cache.attr ^= SCREEN_ATTR_UNSEARCHABLE;
	}
	if (link_state != LINK_STATE_NONE
	    && !elformat.form
	    && html_context->options->underline_links) {
		schar_cache.attr |= SCREEN_ATTR_UNDERLINE;
	}
	// schar_cache.element_offset = (html_top->name && html_context->document && html_context->document->text.source) ? html_top->name - html_context->document->text.source : 0;

	return &schar_cache;
}

void
draw_frame_hchars(struct part *part, int x, int y, int width,
		  unsigned char data, color_T bgcolor, color_T fgcolor,
		  struct html_context *html_context)
{
	struct screen_char *template_;

	assert(width > 0);
	if_assert_failed return;

	template_ = get_frame_char(html_context, part, x + width - 1, y, data, bgcolor, fgcolor);
	if (!template_) return;

	/* The template char is the last we need to draw so only decrease @width. */
	for (width -= 1; width; width--, x++) {
		copy_screen_chars(&POS(x, y), template_, 1);
	}

	if (par_elformat.blockquote_level) {
		draw_blockquote_chars(part, y, html_context);
	}
}

void
draw_blockquote_chars(struct part *part, int y, struct html_context *html_context)
{
	int i;
	int x = par_elformat.orig_leftmargin;
	struct screen_char *const schar = get_format_screen_char(html_context, 0);

	if (LEN(y) + par_elformat.blockquote_level <= x) {
		return;
	}

	schar->data = '>';
	for (i = 1; i < par_elformat.blockquote_level; i++) {
		copy_screen_chars(&POS(x, y), schar, 1);
#ifdef CONFIG_UTF8
		if (part->char_width) part->char_width[x] = 1;
#endif
		x++;
	}
	schar->data = ' ';
	copy_screen_chars(&POS(x, y), schar, 1);
#ifdef CONFIG_UTF8
	if (part->char_width) part->char_width[x] = 1;
#endif
}

void
draw_frame_vchars(struct part *part, int x, int y, int height,
		  unsigned char data, color_T bgcolor, color_T fgcolor,
		  struct html_context *html_context)
{
	struct screen_char *template_ = get_frame_char(html_context, part, x, y,
	                                              data, bgcolor, fgcolor);

	if (!template_) return;

	/* The template char is the first vertical char to be drawn. So
	 * copy it to the rest. */
	for (height -= 1, y += 1; height; height--, y++) {
	    	if (realloc_line(html_context, part->document, Y(y), X(x)) < 0)
			return;

		copy_screen_chars(&POS(x, y), template_, 1);
	}
}

void
expand_lines(struct html_context *html_context, struct part *part,
             int x, int y, int lines, color_T bgcolor)
{
	int line;

	assert(part && part->document);
	if_assert_failed return;

	if (!use_document_bg_colors(&part->document->options) && !par_elformat.blockquote_level)
		return;

	par_elformat.color.background = bgcolor;

	if (x < 0) {
		x = 0;
	}

	for (line = 0; line < lines; line++) {
		if (realloc_line(html_context, part->document, Y(y + line),
			X(x) + par_elformat.blockquote_level + par_elformat.orig_leftmargin) < 0) {
			return;
		}

		if (par_elformat.blockquote_level) {
			draw_blockquote_chars(part, y + line, html_context);
		}
	}
}

/* document.comb_x and document.comb_y exist only when CONFIG_COMBINE
 * is defined.  assert() does nothing if CONFIG_FASTMEM is defined.  */
#if defined(CONFIG_COMBINE) && !defined(CONFIG_FASTMEM)
/** Assert that path->document->comb_x and part->document->comb_y
 * refer to an allocated struct screen_char, or comb_x is -1.
 *
 * The CONFIG_COMBINE variant of set_hline() can update the
 * screen_char.data at these coordinates.  Sometimes, the coordinates
 * have not been valid, and the update has corrupted memory.  These
 * assertions should catch that bug if it happens again.
 *
 * @post This function can leave ::assert_failed set, so the caller
 * should use ::if_assert_failed, perhaps with discard_comb_x_y().  */
static void
assert_comb_x_y_ok(const struct document *document)
{
	assert(document);
	if (document->comb_x != -1) {
		assert(document->comb_y >= 0);
		assert(document->comb_y < document->height);
		assert(document->comb_x >= 0);
		assert(document->comb_x < document->data[document->comb_y].length);
	}
}
#else
# define assert_comb_x_y_ok(document) ((void) 0)
#endif

#ifdef CONFIG_COMBINE
/** Discard any combining characters that have not yet been combined
 * with to the previous base character.  */
void
discard_comb_x_y(struct document *document)
{
	document->comb_x = -1;
	document->comb_y = -1;
	document->combi_length = 0;
}
#endif

#ifdef CONFIG_COMBINE
static void
move_comb_x_y(struct part *part, int xf, int yf, int xt, int yt)
{
	if (part->document->comb_x != -1
	    && part->document->comb_y == Y(yf)
	    && part->document->comb_x >= X(xf)) {
		if (yt >= 0) {
			part->document->comb_x += xt - xf;
			part->document->comb_y += yt - yf;
		} else
			discard_comb_x_y(part->document);
	}
}
#else
# define move_comb_x_y(part, xf, yf, xt, yt) ((void) 0)
#endif

#ifdef CONFIG_COMBINE
static void
set_comb_x_y(struct part *part, int x, int y)
{
	struct document *document = part->document;

	document->comb_x = X(x);
	document->comb_y = Y(y);
	assert_comb_x_y_ok(document);
	if_assert_failed discard_comb_x_y(document);
}
#else
# define set_comb_x_y(part, x, y) ((void) 0)
#endif

#ifdef CONFIG_COMBINE
static void
put_combined(struct part *part, int x)
{
	struct document *document = part->document;

	if (document->combi_length) {
		if (document->comb_x != -1) {
			unicode_val_T prev = get_combined(document->combi, document->combi_length + 1);

			assert_comb_x_y_ok(document);
			if_assert_failed prev = UCS_NO_CHAR;

			/* Make sure the combined character is not considered as
			 * a space. */
			if (x)
				part->spaces[x - 1] = 0;

			if (prev != UCS_NO_CHAR)
				document->data[document->comb_y]
					.ch.chars[document->comb_x].data = prev;
		}
		document->combi_length = 0;
	}
}
#else
# define put_combined(part, x) ((void) 0)
#endif

#ifdef CONFIG_UTF8
/* First possibly do the format change and then find out what coordinates
 * to use since sub- or superscript might change them */
static inline int
set_hline(struct html_context *html_context, const char *chars, int charslen,
	  link_state_T link_state)
{
	struct part *const part = html_context->part;
	struct screen_char *const schar = get_format_screen_char(html_context,
								 link_state);
	int x = part->cx;
	const int y = part->cy;
	const int x2 = x;
	int len = charslen;
	const int utf8 = html_context->options->utf8;
	int orig_length;

	assert(part);
	if_assert_failed return len;

	assert(charslen >= 0);

	if (realloc_spaces(part, x + charslen))
		return 0;

	/* U+00AD SOFT HYPHEN characters in HTML documents are
	 * supposed to be displayed only if the word is broken at that
	 * point.  ELinks currently does not use them, so it should
	 * not display them.  If the input @chars is in UTF-8, then
	 * set_hline() discards the characters.  If the input is in
	 * some other charset, then set_hline() does not know which
	 * byte that charset uses for U+00AD, so it cannot discard
	 * the characters; instead, the translation table used by
	 * convert_string() has already discarded the characters.
	 *
	 * Likewise, if the input @chars is in UTF-8, then it may
	 * contain U+00A0 NO-BREAK SPACE characters; but if the input
	 * is in some other charset, then the translation table
	 * has mapped those characters to NBSP_CHAR.  */

	if (part->document) {
		struct document *const document = part->document;

		assert_comb_x_y_ok(document);
		if_assert_failed discard_comb_x_y(document);

		/* Reallocate LINE(y).chars[] to large enough.  The
		 * last parameter of realloc_line is the index of the
		 * last element to which we may want to write,
		 * i.e. one less than the required size of the array.
		 * Compute the required size by assuming that each
		 * byte of input will need at most one character cell.
		 * (All double-cell characters take up at least two
		 * bytes in UTF-8, and there are no triple-cell or
		 * wider characters.)  However, if there already is an
		 * incomplete character in document->buf, then
		 * the first byte of input can result in a double-cell
		 * character, so we must reserve one extra element.  */
		/* But, ascii replacements complicates this because a single
		   codepoint may be rendered to as much as 20 chars */
		orig_length = realloc_line(html_context, document,
					   Y(y), X(x) + charslen * 20);
		if (orig_length < 0) /* error */
			return 0;

		if (part->begin) {
			if (par_elformat.blockquote_level && !html_context->table_level) {
				draw_blockquote_chars(part, y, html_context);
			}
			part->begin = 0;
		}

		if (utf8) {
			const char *const end = chars + charslen;
			unicode_val_T data;

			if (document->buf_length) {
				/* previous char was broken in the middle */
				int length = utf8charlen(document->buf);
				unsigned char i;
				char *buf_ptr = document->buf;

				for (i = document->buf_length; i < length && chars < end;) {
					document->buf[i++] = *chars++;
				}
				document->buf_length = i;
				document->buf[i] = '\0';
				data = utf8_to_unicode(&buf_ptr, buf_ptr + i);
				if (data != UCS_NO_CHAR) {
					/* FIXME: If there was invalid
					 * UTF-8 in the buffer,
					 * @utf8_to_unicode may have left
					 * some bytes unused.  Those
					 * bytes should be pulled back
					 * into @chars, rather than
					 * discarded.  This is not
					 * trivial to implement because
					 * each byte may have arrived in
					 * a separate call.  */
					document->buf_length = 0;
					goto good_char;
				} else {
					/* Still not full char */
					assert_comb_x_y_ok(document);
					LINE(y).length = orig_length;
					assert_comb_x_y_ok(document);
					if_assert_failed discard_comb_x_y(document);
					return 0;
				}
			}

			while (chars < end) {
				/* ELinks does not use NBSP_CHAR in UTF-8.  */

				data = utf8_to_unicode((char **)&chars, end);
				if (data == UCS_NO_CHAR) {
					part->spaces[x] = 0;
					if (charslen == 1) {
						/* HR */
						unsigned char attr = schar->attr;

						schar->data = (unsigned char)*chars++;
						schar->attr = SCREEN_ATTR_FRAME;
						copy_screen_chars(&POS(x, y), schar, 1);
						schar->attr = attr;
						part->char_width[x++] = 0;
						continue;
					} else {
						unsigned char i;

						for (i = 0; chars < end;i++) {
							document->buf[i] = *chars++;
						}
						document->buf_length = i;
						break;
					}
					/* not reached */
				}

good_char:
				if (data == UCS_SOFT_HYPHEN)
					continue;

				if (data == UCS_NO_BREAK_SPACE
				    && html_context->options->wrap_nbsp)
					data = UCS_SPACE;

#ifdef CONFIG_COMBINE
				if (wcwidth((wchar_t)data)) {
					put_combined(part, x);
					document->combi[0] = data;
				} else {
					if (part->cx == x) {
						if (X(x)) {
							/* Isolated combining 
							 * character not on the 
							 * first column: combine 
							 * it with whatever is 
							 * printed at its left. */
							document->combi[0] = POS(x - 1, y).data;
							set_comb_x_y(part, x - 1, y);
						} else {
							/* Isolated combining 
							 * character on the
							 * first column: use
							 * UCS_NO_BREAK_SPACE as
							 * the base character.
							 * */
							document->combi[0] = UCS_NO_BREAK_SPACE;
							set_comb_x_y(part, x, y);
							schar->data = UCS_SPACE;
							copy_screen_chars(&POS(x++, y), schar, 1);
						}
					}
					if (document->combi_length < (UCS_MAX_LENGTH_COMBINED - 1))
						document->combi[++document->combi_length] = data;
					continue;
				}
#endif /* CONFIG_COMBINE */
				part->spaces[x] = (data == UCS_SPACE);

#ifdef CONFIG_CODEPOINT
				if (codepoint_replacement(data) != -1) {
					int i;

					for(i = 0; i < unicode_to_cell(data); i++) {
						schar->data = encode_utf8(data)[i];
						part->char_width[x] = 1;
						copy_screen_chars(&POS(x++, y), schar, 1);
					}
					continue;
				} else
#endif
				if (unicode_to_cell(data) == 0)
					continue;
				else if (unicode_to_cell(data) == 2) {
					schar->data = (unicode_val_T)data;
					part->char_width[x] = 2;
					copy_screen_chars(&POS(x++, y), schar, 1);
					schar->data = UCS_NO_CHAR;
					part->spaces[x] = 0;
					part->char_width[x] = 0;
				} else {
					part->char_width[x] = unicode_to_cell(data);
					schar->data = (unicode_val_T)data;
				}

				set_comb_x_y(part, x, y);

				copy_screen_chars(&POS(x++, y), schar, 1);
			} /* while chars < end */

			/* Display any trailing combining characters. */
			put_combined(part, x);
		} else { /* not UTF-8 */
			for (; charslen > 0; charslen--, x++, chars++) {
				part->char_width[x] = 1;
				if (*chars == NBSP_CHAR) {
					schar->data = ' ';
					part->spaces[x] = html_context->options->wrap_nbsp;
				} else {
					part->spaces[x] = (*chars == ' ');
					schar->data = *chars;
				}
				copy_screen_chars(&POS(x, y), schar, 1);
			}
		} /* end of UTF-8 check */

		/* Assert that we haven't written past the end of the
		 * LINE(y).chars array.  @x here is one greater than
		 * the last one used in POS(x, y).  Instead of this,
		 * we could assert(X(x) < LINE(y).length) immediately
		 * before each @copy_screen_chars call above, but
		 * those are in an inner loop that should be fast.  */
		assert(X(x) <= LINE(y).length);
		/* Some part of the code is apparently using LINE(y).length
		 * for line-wrapping decisions.  It may currently be too
		 * large because it was allocated above based on @charslen
		 * which is the number of bytes, not the number of cells.
		 * Change the length to the correct size, but don't let it
		 * get smaller than it was on entry to this function.  */
		assert_comb_x_y_ok(document);
		LINE(y).length = int_max(orig_length, X(x));
		assert_comb_x_y_ok(document);
		if_assert_failed discard_comb_x_y(document);
		len = x - x2;
	} else { /* part->document == NULL */
		if (utf8) {
			const char *const end = chars + charslen;

			while (chars < end) {
				unicode_val_T data;

				data = utf8_to_unicode((char **)&chars, end);
#ifdef CONFIG_COMBINE
				if (data == UCS_SOFT_HYPHEN
				    || (data != UCS_NO_CHAR && wcwidth((wchar_t)data) == 0))
#else
				if (data == UCS_SOFT_HYPHEN)
#endif
					continue;

				if (data == UCS_NO_BREAK_SPACE
				    && html_context->options->wrap_nbsp)
					data = UCS_SPACE;
				part->spaces[x] = (data == UCS_SPACE);

				part->char_width[x] = unicode_to_cell(data);
				if (part->char_width[x] == 2) {
					x++;
					part->spaces[x] = 0;
					part->char_width[x] = 0;
				}
				if (data == UCS_NO_CHAR) {
					/* this is at the end only */
					return x - x2;
				}
				x++;
			} /* while chars < end */
			len = x - x2;
		} else { /* not UTF-8 */
			for (; charslen > 0; charslen--, x++, chars++) {
				part->char_width[x] = 1;
				if (*chars == NBSP_CHAR) {
					part->spaces[x] = html_context->options->wrap_nbsp;
				} else {
					part->spaces[x] = (*chars == ' ');
				}
			}
		}
	} /* end of part->document check */
	return len;
}
#else

/* First possibly do the format change and then find out what coordinates
 * to use since sub- or superscript might change them */
static inline void
set_hline(struct html_context *html_context, const char *chars, int charslen,
	  link_state_T link_state)
{
	struct part *part = html_context->part;
	struct screen_char *schar = get_format_screen_char(html_context,
	                                                   link_state);
	int x = part->cx;
	int y = part->cy;

	assert(part);
	if_assert_failed return;

	if (realloc_spaces(part, x + charslen))
		return;

	if (part->document) {
		if (realloc_line(html_context, part->document,
		                 Y(y), X(x) + charslen - 1) < 0)
			return;

		if (part->begin) {
			if (par_elformat.blockquote_level && !html_context->table_level) {
				draw_blockquote_chars(part, y, html_context);
			}
			part->begin = 0;
		}

		for (; charslen > 0; charslen--, x++, chars++) {
			if (*chars == NBSP_CHAR) {
				schar->data = ' ';
				part->spaces[x] = html_context->options->wrap_nbsp;
			} else {
				part->spaces[x] = (*chars == ' ');
				schar->data = *chars;
			}
			copy_screen_chars(&POS(x, y), schar, 1);
		}
	} else {
		for (; charslen > 0; charslen--, x++, chars++) {
			if (*chars == NBSP_CHAR) {
				part->spaces[x] = html_context->options->wrap_nbsp;
			} else {
				part->spaces[x] = (*chars == ' ');
			}
		}
	}
}
#endif /* CONFIG_UTF8 */

static void
move_links(struct html_context *html_context, int xf, int yf, int xt, int yt)
{
	struct part *part;
	struct tag *tag;
	int nlink = renderer_context.last_link_to_move;
	int matched = 0;

	assert(html_context);
	if_assert_failed return;

	part = html_context->part;

	assert(part && part->document);
	if_assert_failed return;

	if (!realloc_lines(part->document, Y(yt)))
		return;

	for (; nlink < part->document->nlinks; nlink++) {
		struct link *link = &part->document->links[nlink];
		int i;

		for (i = 0; i < link->npoints; i++) {
			/* Fix for bug 479 (part one) */
			/* The scenario that triggered it:
			 *
			 * Imagine a centered element containing a really long
			 * word (over half of the screen width long) followed
			 * by a few links with no spaces between them where all
			 * the link text combined with the really long word
			 * will force the line to be wrapped. When rendering
			 * the line first words (including link text words) are
			 * put on one line. Then wrapping is performed moving
			 * all links from current line to the one below. Then
			 * the current line (now only containing the really
			 * long word) is centered. This will trigger a call to
			 * move_links() which will increment.
			 *
			 * Without the fix below the centering of the current
			 * line will increment last_link_to_move to that of the
			 * last link which means centering of the next line
			 * with all the links will only move the last link
			 * leaving all the other links' points dangling and
			 * causing buggy link highlighting.
			 *
			 * Even links like textareas will be correctly handled
			 * because @last_link_to_move is a way to optimize how
			 * many links move_links() will have to iterate and
			 * this little fix will only decrease the effect of the
			 * optimization by always ensuring it is never
			 * incremented too far. */
			if (!matched && link->points[i].y > Y(yf)) {
				matched = 1;
				continue;
			}

			if (link->points[i].y != Y(yf))
				continue;

			matched = 1;

			if (link->points[i].x < X(xf))
				continue;

			if (yt >= 0) {
				link->points[i].y = Y(yt);
				link->points[i].x += -xf + xt;
			} else {
				int to_move = link->npoints - (i + 1);

				assert(to_move >= 0);

				if (to_move > 0) {
					memmove(&link->points[i],
						&link->points[i + 1],
						to_move *
						sizeof(*link->points));
					i--;
				}

				link->npoints--;
			}
		}

		if (!matched) {
			renderer_context.last_link_to_move = nlink;
		}
	}

	/* Don't move tags when removing links. */
	if (yt < 0) return;

	matched = 0;
	tag = renderer_context.last_tag_to_move;

	while (list_has_next(part->document->tags, tag)) {
		tag = tag->next;

		if (tag->y == Y(yf)) {
			matched = 1;
			if (tag->x >= X(xf)) {
				tag->y = Y(yt);
				tag->x += -xf + xt;
			}

		} else if (!matched && tag->y > Y(yf)) {
			/* Fix for bug 479 (part two) */
			matched = 1;
		}

		if (!matched) renderer_context.last_tag_to_move = tag;
	}
}

/* This function does not update document.comb_x and document.comb_y.
 * That is the caller's responsibility.  */
static inline void
copy_chars(struct html_context *html_context, int x, int y, int width, struct screen_char *d)
{
	struct part *part;

	assert(html_context);
	if_assert_failed return;

	part = html_context->part;

	assert(width > 0 && part && part->document && part->document->data);
	if_assert_failed return;

	if (realloc_line(html_context, part->document, Y(y), X(x) + width - 1) < 0)
		return;

	copy_screen_chars(&POS(x, y), d, width);
}

static inline void
move_chars(struct html_context *html_context, int x, int y, int nx, int ny)
{
	struct part *part;

	assert(html_context);
	if_assert_failed return;

	part = html_context->part;

	assert(part && part->document && part->document->data);
	if_assert_failed return;

	if (LEN(y) - x <= 0) return;

	copy_chars(html_context, nx, ny, LEN(y) - x, &POS(x, y));

	assert_comb_x_y_ok(part->document);
	move_comb_x_y(part, x, y, nx, ny);
	LINE(y).length = X(x);
	assert_comb_x_y_ok(part->document);
	if_assert_failed discard_comb_x_y(part->document);
	move_links(html_context, x, y, nx, ny);

	if (par_elformat.blockquote_level && !html_context->table_level) {
		draw_blockquote_chars(part, ny, html_context);
	}
}

/** Shift the line @a y to the right by @a shift character cells,
 * and update document.comb_x and document.comb_y.  */
static inline void
shift_chars(struct html_context *html_context, int y, int shift)
{
	struct part *part;
	struct screen_char *a;
	int len;

	assert(html_context);
	if_assert_failed return;

	part = html_context->part;

	assert(part && part->document && part->document->data);
	if_assert_failed return;

	len = LEN(y);

	a = (struct screen_char *)fmem_alloc(len * sizeof(*a));
	if (!a) return;

	copy_screen_chars(a, &POS(0, y), len);

	assert_comb_x_y_ok(part->document);
	if_assert_failed discard_comb_x_y(part->document);

	clear_hchars(html_context, 0, y, shift, a);
	copy_chars(html_context, shift, y, len, a);
	fmem_free(a);

	move_links(html_context, 0, y, shift, y);
	move_comb_x_y(part, 0, y, shift, y);

	assert_comb_x_y_ok(part->document);
	if_assert_failed discard_comb_x_y(part->document);
}

static inline void
del_chars(struct html_context *html_context, int x, int y)
{
	struct part *part;

	assert(html_context);
	if_assert_failed return;

	part = html_context->part;

	assert(part && part->document && part->document->data);
	if_assert_failed return;

	assert_comb_x_y_ok(part->document);
	if_assert_failed discard_comb_x_y(part->document);

	LINE(y).length = X(x);
	move_comb_x_y(part, x, y, -1, -1);
	move_links(html_context, x, y, -1, -1);

	assert_comb_x_y_ok(part->document);
	if_assert_failed discard_comb_x_y(part->document);
}

#if TABLE_LINE_PADDING < 0
# define overlap_width(x) (x).width
#else
# define overlap_width(x) int_min((x).width, \
	html_context->options->document_width - TABLE_LINE_PADDING)
#endif
#define overlap(x) int_max(overlap_width(x) - (x).rightmargin, 0)

static int inline
split_line_at(struct html_context *html_context, int width)
{
	struct part *part;
	int tmp;
	int new_width = width + par_elformat.rightmargin;

	assert(html_context);
	if_assert_failed return 0;

	part = html_context->part;

	assert(part);
	if_assert_failed return 0;

	/* Make sure that we count the right margin to the total
	 * actual box width. */
	int_lower_bound(&part->box.width, new_width);

	if (part->document) {
		assert(part->document->data);
		if_assert_failed return 0;
#ifdef CONFIG_UTF8
		if (html_context->options->utf8
		    && width < part->spaces_len && part->char_width[width] == 2) {
			move_chars(html_context, width, part->cy, par_elformat.leftmargin + par_elformat.blockquote_level * (html_context->table_level == 0), part->cy + 1);
			del_chars(html_context, width, part->cy);
		} else
#endif
		{
			assertm(POS(width, part->cy).data == ' ',
					"bad split: %c", POS(width, part->cy).data);
			move_chars(html_context, width + 1, part->cy, par_elformat.leftmargin + par_elformat.blockquote_level  * (html_context->table_level == 0), part->cy + 1);
			del_chars(html_context, width, part->cy);
		}
	}

#ifdef CONFIG_UTF8
	if (!(html_context->options->utf8
	      && width < part->spaces_len
	      && part->char_width[width] == 2))
#endif
		width++; /* Since we were using (x + 1) only later... */

	tmp = part->spaces_len - width;
	if (tmp > 0) {
		/* 0 is possible and I'm paranoid ... --Zas */
		memmove(part->spaces, part->spaces + width, tmp);
#ifdef CONFIG_UTF8
		memmove(part->char_width, part->char_width + width, tmp);
#endif
	}

	assert(tmp >= 0);
	if_assert_failed tmp = 0;
	memset(part->spaces + tmp, 0, width);
#ifdef CONFIG_UTF8
	memset(part->char_width + tmp, 0, width);
#endif

	if (par_elformat.leftmargin + par_elformat.blockquote_level  * (html_context->table_level == 0) > 0) {
		tmp = part->spaces_len - (par_elformat.leftmargin + par_elformat.blockquote_level * (html_context->table_level == 0));
		assertm(tmp > 0, "part->spaces_len - par_elformat.leftmargin == %d", tmp);
		/* So tmp is zero, memmove() should survive that. Don't recover. */
		memmove(part->spaces + par_elformat.leftmargin + par_elformat.blockquote_level * (html_context->table_level == 0), part->spaces, tmp);
#ifdef CONFIG_UTF8
		memmove(part->char_width + par_elformat.leftmargin + par_elformat.blockquote_level * (html_context->table_level == 0), part->char_width, tmp);
#endif
	}

	part->cy++;

	if (part->cx == width) {
		part->cx = -1;
		int_lower_bound(&part->box.height, part->cy);
		return 2;
	} else {
		part->cx -= width - (par_elformat.leftmargin + par_elformat.blockquote_level  * (html_context->table_level == 0));
		int_lower_bound(&part->box.height, part->cy + 1);
		return 1;
	}
}

/* Here, we scan the line for a possible place where we could split it into two
 * (breaking it, because it is too long), if it is overlapping from the maximal
 * box width. */
/* Returns 0 if there was found no spot suitable for breaking the line.
 *         1 if the line was split into two.
 *         2 if the (second) splitted line is blank (that is useful to determine
 *           ie. if the next line_break() should really break the line; we don't
 *           want to see any blank lines to pop up, do we?). */
static int
split_line(struct html_context *html_context)
{
	struct part *part;
	int x;

	assert(html_context);
	if_assert_failed return 0;

	part = html_context->part;

	assert(part);
	if_assert_failed return 0;

#ifdef CONFIG_UTF8
	if (html_context->options->utf8) {
		for (x = overlap(par_elformat); x >= (par_elformat.leftmargin + par_elformat.blockquote_level * (html_context->table_level == 0)); x--) {

			if (x < part->spaces_len && (part->spaces[x]
			    || (part->char_width[x] == 2
				/* Ugly hack. If we haven't place for
				 * double-width characters we print two
				 * double-width characters. */
				&& x != (par_elformat.leftmargin + par_elformat.blockquote_level * (html_context->table_level == 0)))))
				return split_line_at(html_context, x);
		}

		for (x = par_elformat.leftmargin + par_elformat.blockquote_level * (html_context->table_level == 0); x < part->cx ; x++) {
			if (x < part->spaces_len && (part->spaces[x]
			    || (part->char_width[x] == 2
				/* We want to break line after _second_
				 * double-width character. */
				&& x > (par_elformat.leftmargin + par_elformat.blockquote_level * (html_context->table_level == 0)))))
				return split_line_at(html_context, x);
		}
	} else
#endif
	{
		for (x = overlap(par_elformat); x >= (par_elformat.leftmargin + par_elformat.blockquote_level * (html_context->table_level == 0)); x--)
			if (x < part->spaces_len && part->spaces[x])
				return split_line_at(html_context, x);

		for (x = par_elformat.leftmargin + par_elformat.blockquote_level * (html_context->table_level == 0); x < part->cx ; x++)
			if (x < part->spaces_len && part->spaces[x])
				return split_line_at(html_context, x);
	}

	/* Make sure that we count the right margin to the total
	 * actual box width. */
	int_lower_bound(&part->box.width, part->cx + par_elformat.rightmargin);

	return 0;
}

/* Insert @new_spaces spaces before the coordinates @x and @y,
 * adding those spaces to whatever link is at those coordinates. */
/* TODO: Integrate with move_links. */
static void
insert_spaces_in_link(struct part *part, int x, int y, int new_spaces)
{
	int i = part->document->nlinks;

	x = X(x);
	y = Y(y);

	while (i--) {
		struct link *link = &part->document->links[i];
		int j = link->npoints;

		while (j-- > 1) {
			struct point *point = &link->points[j];

			if (point->x != x || point->y != y)
				continue;

			if (!realloc_points(link, link->npoints + new_spaces))
				return;

			link->npoints += new_spaces;
			point = &link->points[link->npoints - 1];

			while (new_spaces--) {
				point->x = --x;
				point->y = y;
				point--;
			}

			return;
		}
	}
}

/* This function is very rare exemplary of clean and beautyful code here.
 * Please handle with care. --pasky */
static void
justify_line(struct html_context *html_context, int y)
{
	struct part *part;
	struct screen_char *line; /* we save original line here */
	int len;
	int pos;
	int *space_list;
	int spaces;
	int diff;

	assert(html_context);
	if_assert_failed return;

	part = html_context->part;

	assert(part && part->document && part->document->data);
	if_assert_failed return;

	len = LEN(y);
	assert(len > 0);
	if_assert_failed return;

	line = (struct screen_char *)fmem_alloc(len * sizeof(*line));
	if (!line) return;

	/* It may sometimes happen that the line is only one char long and that
	 * char is space - then we're going to write to both [0] and [1], but
	 * we allocated only one field. Thus, we've to do (len + 1). --pasky */
	space_list = (int *)fmem_alloc((len + 1) * sizeof(*space_list));
	if (!space_list) {
		fmem_free(line);
		return;
	}

	copy_screen_chars(line, &POS(0, y), len);

	/* Skip leading spaces */

	spaces = 0;
	pos = 0;

	while (line[pos].data == ' ')
		pos++;

	/* Yes, this can be negative, we know. But we add one to it always
	 * anyway, so it's ok. */
	space_list[spaces++] = pos - 1;

	/* Count spaces */

	for (; pos < len; pos++)
		if (line[pos].data == ' ')
			space_list[spaces++] = pos;

	space_list[spaces] = len;

	/* Realign line */

	/* Diff is the difference between the width of the paragraph
	 * and the current length of the line. */
	diff = overlap(par_elformat) - len;

	/* We check diff > 0 because diff can be negative (i.e., we have
	 * an unbroken line of length > overlap(par_elformat))
	 * even when spaces > 1 if the line has only non-breaking spaces. */
	if (spaces > 1 && diff > 0) {
		int prev_end = 0;
		int word;

		/* Allocate enough memory for the justified line.
		 * If the memory is not available, then leave the
		 * line unchanged, rather than halfway there.  The
		 * following loop assumes the allocation succeeded.  */
		if (!realloc_line(html_context, html_context->part->document,
				  Y(y), X(overlap(par_elformat))))
			goto out_of_memory;

		for (word = 0; word < spaces; word++) {
			/* We have to increase line length by 'diff' num. of
			 * characters, so we move 'word'th word 'word_shift'
			 * characters right. */
			int word_start = space_list[word] + 1;
			int word_len = space_list[word + 1] - word_start;
			int word_shift;
			int new_start;
			int new_spaces;

			assert(word_len >= 0);
			if_assert_failed continue;

			word_shift = (word * diff) / (spaces - 1);
			new_start = word_start + word_shift;

			/* Assert that the realloc_line() above
			 * allocated enough memory for the word
			 * and the preceding spaces.  */
			assert(LEN(y) >= new_start + word_len);
			if_assert_failed continue;

			/* Copy the original word, without any spaces.
			 * word_len may be 0 here.  */
			copy_screen_chars(&POS(new_start, y),
					  &line[word_start], word_len);

			/* Copy the space that preceded the word,
			 * duplicating it as many times as necessary.
			 * This preserves its attributes, such as
			 * background color and underlining.  If this
			 * is the first word, then skip the copy
			 * because there might not be a space there
			 * and anyway it need not be duplicated.  */
			if (word) {
				int spacex;

				for (spacex = prev_end; spacex < new_start;
				     ++spacex) {
					copy_screen_chars(&POS(spacex, y),
							  &line[word_start - 1],
							  1);
				}
			}

			/* Remember that any links at the right side
			 * of the added spaces have moved, and the
			 * spaces themselves may also belong to a
			 * link.  */
			new_spaces = new_start - prev_end - 1;
			if (word && new_spaces) {
				move_comb_x_y(part, prev_end + 1, y, new_start, y);
				move_links(html_context, prev_end + 1, y, new_start, y);
				insert_spaces_in_link(part,
						      new_start, y, new_spaces);
			}

			prev_end = new_start + word_len;
		}
	}

out_of_memory:
	fmem_free(space_list);
	fmem_free(line);
}

static void
align_line(struct html_context *html_context, int y, int last)
{
	struct part *part;
	int shift;
	int len;

	assert(html_context);
	if_assert_failed return;

	part = html_context->part;

	assert(part && part->document && part->document->data);
	if_assert_failed return;

	len = LEN(y);

	if (!len || par_elformat.align == ALIGN_LEFT)
		return;

	if (par_elformat.align == ALIGN_JUSTIFY) {
		if (!last)
			justify_line(html_context, y);
		return;
	}

	shift = overlap(par_elformat) - len;
	if (par_elformat.align == ALIGN_CENTER)
		shift /= 2;
	if (shift > 0)
		shift_chars(html_context, y, shift);
}

static inline void
init_link_event_hooks(struct html_context *html_context, struct link *link)
{
	link->event_hooks = (LIST_OF(struct script_event_hook) *)mem_calloc(1, sizeof(*link->event_hooks));
	if (!link->event_hooks) return;

#define add_evhook(list_, type_, src_)						\
	do {									\
		struct script_event_hook *evhook;				\
										\
		if (!src_) break;						\
										\
		evhook = (struct script_event_hook *)mem_calloc(1, sizeof(*evhook));			\
		if (!evhook) break;						\
										\
		evhook->type = type_;						\
		evhook->src  = stracpy(src_);					\
		add_to_list(*(list_), evhook);					\
	} while (0)

	init_list(*link->event_hooks);
	add_evhook(link->event_hooks, SEVHOOK_ONCLICK, elformat.onclick);
	add_evhook(link->event_hooks, SEVHOOK_ONDBLCLICK, elformat.ondblclick);
	add_evhook(link->event_hooks, SEVHOOK_ONMOUSEOVER, elformat.onmouseover);
	add_evhook(link->event_hooks, SEVHOOK_ONHOVER, elformat.onhover);
	add_evhook(link->event_hooks, SEVHOOK_ONFOCUS, elformat.onfocus);
	add_evhook(link->event_hooks, SEVHOOK_ONMOUSEOUT, elformat.onmouseout);
	add_evhook(link->event_hooks, SEVHOOK_ONBLUR, elformat.onblur);
	add_evhook(link->event_hooks, SEVHOOK_ONKEYDOWN, elformat.onkeydown);
	add_evhook(link->event_hooks, SEVHOOK_ONKEYUP, elformat.onkeyup);
	add_evhook(link->event_hooks, SEVHOOK_ONKEYPRESS, elformat.onkeypress);

#ifdef CONFIG_ECMASCRIPT
	add_evhook(link->event_hooks, SEVHOOK_ONKEYPRESS_BODY, html_context->document->body_onkeypress);
#endif
#undef add_evhook
}

static struct link *
new_link(struct html_context *html_context, const char *name, int namelen)
{
	struct document *document;
	struct part *part;
	int link_number;
	struct link *link;

	assert(html_context);
	if_assert_failed return NULL;

	part = html_context->part;

	assert(part);
	if_assert_failed return NULL;

	document = part->document;

	assert(document);
	if_assert_failed return NULL;

	link_number = part->link_num;

	if (!ALIGN_LINK(&document->links, document->nlinks, document->nlinks + 1))
		return NULL;

	link = &document->links[document->nlinks++];
	link->number = link_number - 1;
	if (document->options.use_tabindex) link->number += elformat.tabindex;
	link->accesskey = elformat.accesskey;
	link->title = null_or_stracpy(elformat.title);
	link->where_img = null_or_stracpy(elformat.image);

	if (!elformat.form) {
		link->target = null_or_stracpy(elformat.target);
		link->data.name = memacpy(name, namelen);
		/* if (strlen(url) > 4 && !c_strncasecmp(url, "MAP@", 4)) { */
		if (elformat.link
		    && ((elformat.link[0]|32) == 'm')
		    && ((elformat.link[1]|32) == 'a')
		    && ((elformat.link[2]|32) == 'p')
		    && 	(elformat.link[3]     == '@')
		    &&   elformat.link[4]) {
			link->type = LINK_MAP;
			link->where = stracpy(elformat.link + 4);
		} else {
			link->type = LINK_HYPERTEXT;
			link->where = null_or_stracpy(elformat.link);
		}

	} else {
		struct el_form_control *fc = elformat.form;
		struct form *form;

		switch (fc->type) {
		case FC_TEXT:
		case FC_PASSWORD:
		case FC_FILE:
			link->type = LINK_FIELD;
			break;
		case FC_TEXTAREA:
			link->type = LINK_AREA;
			break;
		case FC_CHECKBOX:
		case FC_RADIO:
			link->type = LINK_CHECKBOX;
			break;
		case FC_SELECT:
			link->type = LINK_SELECT;
			break;
		case FC_SUBMIT:
		case FC_IMAGE:
		case FC_RESET:
		case FC_BUTTON:
		case FC_HIDDEN:
			link->type = LINK_BUTTON;
			break;
		default:
			break;
		}
		link->data.form_control = fc;
		/* At this point, elformat.form might already be set but
		 * the form_control not registered through SP_CONTROL
		 * yet, therefore without fc->form set. It is always
		 * after the "good" last form was already processed,
		 * though, so we can safely just take that. */
		form = fc->form;
		if (!form && !list_empty(document->forms))
			form = (struct form *)document->forms.next;
		link->target = null_or_stracpy(form ? form->target : NULL);
	}

	link->color.background = elformat.style.color.background;
	link->color.foreground = link_is_textinput(link)
				? elformat.style.color.foreground
				: elformat.color.clink;

#ifdef CONFIG_ECMASCRIPT
	link->element_offset = (elformat.top_name && document->text.source) ? elformat.top_name - document->text.source : 0;
#endif

	init_link_event_hooks(html_context, link);

	document->links_sorted = 0;
	return link;
}

static void
html_special_tag(struct document *document, char *t, int x, int y)
{
	struct tag *tag;
	int tag_len;

	assert(document);
	if_assert_failed return;

	tag_len = strlen(t);
	/* One byte is reserved for name in struct tag. */
	tag = (struct tag *)mem_alloc(sizeof(*tag) + tag_len);
	if (!tag) return;

	tag->x = x;
	tag->y = y;
	memcpy(tag->name, t, tag_len + 1);
	add_to_list(document->tags, tag);
	if (renderer_context.last_tag_for_newline == (struct tag *) &document->tags)
		renderer_context.last_tag_for_newline = tag;
}


void
put_chars_conv(struct html_context *html_context,
               const char *chars, int charslen)
{
	assert(html_context);
	if_assert_failed return;

	assert(html_context->part && chars && charslen);
	if_assert_failed return;

	if (elformat.style.attr & AT_GRAPHICS) {
		put_chars(html_context, chars, charslen);
		return;
	}

	convert_string(renderer_context.convert_table, chars, charslen,
	               html_context->options->cp,
	               (elformat.style.attr & AT_NO_ENTITIES) ? CSM_NONE : CSM_DEFAULT,
		       NULL, (void (*)(void *, char *, int)) put_chars, html_context);
}

/*
 * Converts a number in base 10 to a string in another base whose symbols are
 * represented by key. I the trivial case, key="0123456789". A more homerow
 * friendly key="gfdsahjkl;trewqyuiopvcxznm". Returns the length of link_sym.
 */
int
dec2qwerty(int num, char *link_sym, const char *key, int base)
{
	int newlen, i, pow;

	if (base < 2) return 0;

	for (newlen = 1, pow = base; pow <= num; ++newlen, pow *= base);

	link_sym[newlen] = '\0';
	for (i = 1; i <= newlen; ++i) {
		int key_index = num % base;
		link_sym[newlen - i] = key[key_index];
		num /= base;
	}
	return newlen;
}

/*
 * Returns the value of link_sym in decimal according to key.
 */
int
qwerty2dec(const char *link_sym, const char *key, int base)
{
	int z = 0;
	int symlen = strlen(link_sym);
	int i;
	int pow;

	for (i = 0, pow = 1; i < symlen; ++i, pow *= base) {
		int j = 0;
		while (key[j] != link_sym[symlen - 1 - i]) ++j;
		z += j * pow;
	}
	return z;
}

static inline void
put_link_number(struct html_context *html_context)
{
	char *symkey = get_opt_str("document.browse.links.label_key", NULL);
	struct part *part = html_context->part;
	char s[64];
	char *fl = elformat.link;
	char *ft = elformat.target;
	char *fi = elformat.image;
	struct text_style old_style = elformat.style;
	struct el_form_control *ff = elformat.form;
	int slen = 0;
	int base = strlen(symkey);

	elformat.link = elformat.target = elformat.image = NULL;
	elformat.form = NULL;
	if (html_context->options->use_link_number_color) {
		elformat.style.attr &= ~AT_BOLD;
		elformat.style.color.foreground = elformat.color.link_number;
	}

	s[slen++] = '[';
	slen += dec2qwerty(part->link_num, s + 1, symkey, base);
	s[slen++] = ']';
	s[slen] = '\0';

	renderer_context.nosearchable = 1;
	put_chars(html_context, s, slen);
	renderer_context.nosearchable = 0;

	if (ff && ff->type == FC_TEXTAREA) line_break(html_context);

	/* We might have ended up on a new line after the line breaking
	 * or putting the link number chars. */
	if (part->cx == -1) {
		part->cx = par_elformat.leftmargin + par_elformat.blockquote_level * (html_context->table_level == 0);
		part->begin = 1;
	}

	elformat.link = fl;
	elformat.target = ft;
	elformat.image = fi;
	elformat.form = ff;
	elformat.style = old_style;
}

#define assert_link_variable(old, new) \
	assertm(!(old), "Old link value [%s]. New value [%s]", old, new);

static inline void
init_link_state_info(char *link, char *target,
		     char *image, struct el_form_control *form)
{
	assert_link_variable(renderer_context.link_state_info.image, image);
	assert_link_variable(renderer_context.link_state_info.target, target);
	assert_link_variable(renderer_context.link_state_info.link, link);

	renderer_context.link_state_info.link = null_or_stracpy(link);
	renderer_context.link_state_info.target = null_or_stracpy(target);
	renderer_context.link_state_info.image = null_or_stracpy(image);
	renderer_context.link_state_info.form = form;
}

static inline void
done_link_state_info(void)
{
	mem_free_if(renderer_context.link_state_info.link);
	mem_free_if(renderer_context.link_state_info.target);
	mem_free_if(renderer_context.link_state_info.image);
	memset(&renderer_context.link_state_info, 0,
	       sizeof(renderer_context.link_state_info));
}

#ifdef CONFIG_UTF8
static inline void
process_link(struct html_context *html_context, link_state_T link_state,
	     const char *chars, int charslen, int cells)
#else
static inline void
process_link(struct html_context *html_context, link_state_T link_state,
		   const char *chars, int charslen)
#endif /* CONFIG_UTF8 */
{
	struct part *part = html_context->part;
	struct link *link;
	int x_offset = 0;

	switch (link_state) {
	case LINK_STATE_SAME: {
		char *name;

		if (!part->document) return;

		assertm(part->document->nlinks > 0, "no link");
		if_assert_failed return;

		link = &part->document->links[part->document->nlinks - 1];

		name = get_link_name(link);
		if (name) {
			char *new_name;

			new_name = straconcat(name, chars,
					      (char *) NULL);
			if (new_name) {
				mem_free(name);
				link->data.name = new_name;
			}
		}

		/* FIXME: Concatenating two adjectent <a> elements to a single
		 * link is broken since we lose the event handlers for the
		 * second one.  OTOH simply appending them here won't fly since
		 * we may get here multiple times for even a single link. We
		 * will probably need some SP_ for creating a new link or so.
		 * --pasky */

		break;
	}

	case LINK_STATE_NEW:
		part->link_num++;

		init_link_state_info(elformat.link, elformat.target,
				     elformat.image, elformat.form);
		if (!part->document) return;

		/* Trim leading space from the link text */
		while (x_offset < charslen && (unsigned char)chars[x_offset] <= ' ')
			x_offset++;

		if (x_offset) {
			charslen -= x_offset;
			chars += x_offset;
#ifdef CONFIG_UTF8
			cells -= x_offset;
#endif /* CONFIG_UTF8 */
		}

		link = new_link(html_context, chars, charslen);
		if (!link) return;

		break;

	case LINK_STATE_NONE:
	default:
		INTERNAL("bad link_state %i", (int) link_state);
		return;
	}

	/* Add new canvas positions to the link. */
#ifdef CONFIG_UTF8
	if (realloc_points(link, link->npoints + cells))
#else
	if (realloc_points(link, link->npoints + charslen))
#endif /* CONFIG_UTF8 */
	{
		struct point *point = &link->points[link->npoints];
		int x = X(part->cx) + x_offset;
		int y = Y(part->cy);

#ifdef CONFIG_UTF8
		link->npoints += cells;

		for (; cells > 0; cells--, point++, x++)
#else
		link->npoints += charslen;

		for (; charslen > 0; charslen--, point++, x++)
#endif /* CONFIG_UTF8 */
		{
			point->x = x;
			point->y = y;
		}
	}
}

static inline link_state_T
get_link_state(struct html_context *html_context)
{
	link_state_T state;

	if (!(elformat.link || elformat.image || elformat.form)) {
		state = LINK_STATE_NONE;

	} else if ((renderer_context.link_state_info.link
		    || renderer_context.link_state_info.image
		    || renderer_context.link_state_info.form)
		   && !xstrcmp(elformat.link, renderer_context.link_state_info.link)
		   && !xstrcmp(elformat.target, renderer_context.link_state_info.target)
		   && !xstrcmp(elformat.image, renderer_context.link_state_info.image)
		   && elformat.form == renderer_context.link_state_info.form) {

		return LINK_STATE_SAME;

	} else {
		state = LINK_STATE_NEW;
	}

	done_link_state_info();

	return state;
}

static inline int
html_has_non_space_chars(const char *chars, int charslen)
{
	int pos = 0;

	while (pos < charslen)
		if (!isspace((unsigned char)chars[pos++]))
			return 1;

	return 0;
}

static void
put_chars(struct html_context *html_context, const char *chars, int charslen)
{
	link_state_T link_state;
	struct part *part;
#ifdef CONFIG_UTF8
	int cells;
#endif /* CONFIG_UTF8 */

	assert(html_context);
	if_assert_failed return;

	part = html_context->part;

	assert(part);
	if_assert_failed return;

	assert(chars && charslen);
	if_assert_failed return;

	/* If we are not handling verbatim aligning and we are at the begining
	 * of a line trim whitespace. */
	if (part->cx == -1) {
		/* If we are not handling verbatim aligning trim leading
		 * whitespaces. */
		if (!html_is_preformatted()) {
			while (charslen && *chars == ' ') {
				chars++;
				charslen--;
			}

			if (charslen < 1) return;
		}

		part->cx = par_elformat.leftmargin + (par_elformat.blockquote_level * (html_context->table_level == 0));
		part->begin = 1;
	}

	/* For preformatted html always update 'the last tag' so we never end
	 * up moving tags to the wrong line (Fixes bug 324). For all other html
	 * it is moved only when the line being rendered carry some real
	 * non-whitespace content. */
	if (html_is_preformatted()
	    || html_has_non_space_chars(chars, charslen)) {
		renderer_context.last_tag_for_newline = (struct tag *) &part->document->tags;
	}

	int_lower_bound(&part->box.height, part->cy + 1);

	link_state = get_link_state(html_context);

	if (link_state == LINK_STATE_NEW) {
		int x_offset = 0;

		/* Don't add inaccessible links. It seems to be caused
		 * by the parser putting a space char after stuff like
		 * <img>-tags or comments wrapped in <a>-tags. See bug
		 * 30 for test case. */
		while (x_offset < charslen && (unsigned char)chars[x_offset] <= ' ')
			x_offset++;

		/* For pure spaces reset the link state */
		if (x_offset == charslen)
			link_state = LINK_STATE_NONE;
		else if (html_context->options->links_numbering)
			put_link_number(html_context);
	}
#ifdef CONFIG_UTF8
	cells =
#endif /* CONFIG_UTF8 */
		set_hline(html_context, chars, charslen, link_state);

	if (link_state != LINK_STATE_NONE) {
#ifdef CONFIG_UTF8
		process_link(html_context, link_state, chars, charslen,
			     cells);
#else
		process_link(html_context, link_state, chars, charslen);
#endif /* CONFIG_UTF8 */
	}

#ifdef CONFIG_UTF8
	if (renderer_context.nowrap
	    && part->cx + cells > overlap(par_elformat))
		return;

	part->cx += cells;
#else
	if (renderer_context.nowrap
			&& part->cx + charslen > overlap(par_elformat))
		return;

	part->cx += charslen;
#endif /* CONFIG_UTF8 */

	renderer_context.nobreak = 0;

	if (html_context->options->wrap || !html_is_preformatted()) {
		while (part->cx > overlap(par_elformat)
		       && part->cx > (par_elformat.leftmargin + par_elformat.blockquote_level  * (html_context->table_level == 0))) {
			int x = split_line(html_context);

			if (!x) break;
			if (part->document)
				align_line(html_context, part->cy - 1, 0);
			renderer_context.nobreak = !!(x - 1);
		}
	}

	assert(charslen > 0);
#ifdef CONFIG_UTF8
	part->xa += cells;
#else
	part->xa += charslen;
#endif /* CONFIG_UTF8 */
	int_lower_bound(&part->max_width, part->xa
			+ par_elformat.leftmargin + par_elformat.blockquote_level * (html_context->table_level == 0) + par_elformat.rightmargin
			- (chars[charslen - 1] == ' '
			   && (html_context->options->wrap || !html_is_preformatted())));
}

#undef overlap

void
line_break(struct html_context *html_context)
{
	struct part *part;
	struct tag *tag;

	assert(html_context);
	if_assert_failed return;

	part = html_context->part;

	assert(part);
	if_assert_failed return;

	int_lower_bound(&part->box.width, part->cx + par_elformat.rightmargin);

	if (renderer_context.nobreak) {
		renderer_context.nobreak = 0;
		part->cx = -1;
		part->xa = 0;
		return;
	}

	if (!part->document || !part->document->data) goto end;

	if (!realloc_lines(part->document, part->box.height + part->cy + 1))
		return;

	if (part->cx > (par_elformat.leftmargin + par_elformat.blockquote_level * (html_context->table_level == 0)) && LEN(part->cy) > part->cx - 1
	    && POS(part->cx - 1, part->cy).data == ' ') {
		del_chars(html_context, part->cx - 1, part->cy);
		part->cx--;
	}

	if (part->cx > 0) align_line(html_context, part->cy, 1);

	for (tag = renderer_context.last_tag_for_newline;
	     tag && tag != (struct tag *) &part->document->tags;
	     tag = tag->prev) {
		tag->x = X(0);
		tag->y = Y(part->cy + 1);
	}

end:
	part->cy++;
	part->cx = -1;
	part->xa = 0;
   	memset(part->spaces, 0, part->spaces_len);
#ifdef CONFIG_UTF8
   	memset(part->char_width, 0, part->spaces_len);
#endif
}

static void
html_special_form(struct part *part, struct form *form)
{
	struct form *nform;

	assert(part && form);
	assert(form->form_num > 0);
	assert(form->form_end == INT_MAX);
	if_assert_failed return;

	if (!part->document) {
		done_form(form);
		return;
	}

	/* Make a fake form with form_num == 0 so that there is
	 * something to use if form controls appear above the first
	 * actual FORM element.  There can never be a real form with
	 * form_num == 0 because the form_num is the position after the
	 * "<form" characters and that's already five characters.  The
	 * fake form does not have a name, and it gets a form_view and
	 * becomes visible to ECMAScript only if it actually has
	 * controls in it.  */
	if (list_empty(part->document->forms)) {
		nform = init_form();
		if (!nform) {
			done_form(form);
			return;
		}
		nform->form_num = 0;
		add_to_list(part->document->forms, nform);
	}

	/* Make sure the new form ``claims'' its slice of the form range
	 * maintained in the form_num and form_end variables. */
	foreach (nform, part->document->forms) {
		if (form->form_num < nform->form_num
		    || nform->form_end < form->form_num)
			continue;

		/* First check if the form has identical form numbers.
		 * That should only be the case when the form being
		 * added is in fact the same form in which case it
		 * should be dropped. The fact that this can happen
		 * suggests that the table renderering can be confused.
		 * See bug 647 for a test case.
		 * Do not compare form->form_end here because it is
		 * normally set by this function and that has obviously
		 * not yet been done.  */
		if (nform->form_num == form->form_num) {
			done_form(form);
			return;
		}

		/* The form start is inside an already added form, so
		 * partition the space of the existing form and get
		 * |old|new|. */
		form->form_end = nform->form_end;
		nform->form_end = form->form_num - 1;
		assertm(nform->form_num <= nform->form_end,
			"[%d:%d] [%d:%d]", nform->form_num, nform->form_end,
			form->form_num, form->form_end);
		add_to_list(part->document->forms, form);
		return;
	}

	ERROR("hole between forms");
	done_form(form);
	return;
}

static void
html_special_form_control(struct part *part, struct el_form_control *fc)
{
	struct form *form;

	assert(part && fc);
	if_assert_failed return;

	if (!part->document) {
		done_form_control(fc);
		mem_free(fc);
		return;
	}

	fc->g_ctrl_num = renderer_context.g_ctrl_num++;

	if (list_empty(part->document->forms)) {
		/* No forms encountered yet, that means a homeless form
		 * control. Generate a dummy form for those Flying
		 * Dutchmans. */
		form = init_form();
		form->form_num = 0;
		add_to_list(part->document->forms, form);
	}
	/* Attach this form control to the last form encountered. */
	form = (struct form *)part->document->forms.next;
	fc->form = form;
	add_to_list(form->items, fc);
}

#ifdef CONFIG_DEBUG
/** Assert that each form in the list has a different form.form_num
 * ... form.form_end range and that the ranges are contiguous and
 * together cover all numbers from 0 to INT_MAX.  Alternatively, the
 * whole list may be empty.  This function can be called from a
 * debugger, or automatically from some places.
 *
 * This function may leave assert_failed = 1; the caller must use
 * if_assert_failed.  */
static void
assert_forms_list_ok(LIST_OF(struct form) *forms)
{
	int saw_form_num_0 = 0;
	struct form *outer;

	if (list_empty(*forms)) return;

	/* O(n^2) algorithm, but it's only for debugging.  */
	foreach (outer, *forms) {
		int followers = 0;
		struct form *inner;

		if (outer->form_num == 0)
			saw_form_num_0++;

		foreach (inner, *forms) {
			assert(inner == outer
			       || inner->form_num > outer->form_end
			       || outer->form_num > inner->form_end);
			if (outer->form_end == inner->form_num - 1)
				followers++;
		}

		if (outer->form_end == INT_MAX)
			assert(followers == 0);
		else
			assert(followers == 1);
	}

//	assert(saw_form_num_0 == 1);
}
#else  /* !CONFIG_DEBUG */
# define assert_forms_list_ok(forms) ((void) 0)
#endif /* !CONFIG_DEBUG */

/* Reparents form items based on position in the source. */
void
check_html_form_hierarchy(struct part *part)
{
	struct document *document = part->document;
	INIT_LIST_OF(struct el_form_control, form_controls);
	struct form *form;
	struct el_form_control *fc, *next;

	if (list_empty(document->forms))
		return;

	assert_forms_list_ok(&document->forms);
	if_assert_failed {}

	/* Take out all badly placed form items. */

	foreach (form, document->forms) {

		assertm(form->form_num <= form->form_end,
			"%p [%d : %d]", form, form->form_num, form->form_end);

		foreachsafe (fc, next, form->items) {
			if (form->form_num <= fc->position
			    && fc->position <= form->form_end)
				continue;

			move_to_top_of_list(form_controls, fc);
		}
	}

	/* Re-insert the form items the correct places. */

	foreachsafe (fc, next, form_controls) {

		foreach (form, document->forms) {
			if (fc->position < form->form_num
			    || form->form_end < fc->position)
				continue;

			fc->form = form;
			move_to_top_of_list(form->items, fc);
			break;
		}
	}

	assert(list_empty(form_controls));
}

static inline void
color_link_lines(struct html_context *html_context)
{
	struct document *document = html_context->part->document;
	struct color_pair colors = INIT_COLOR_PAIR(par_elformat.color.background, 0x0);
	color_mode_T color_mode = document->options.color_mode;
	color_flags_T color_flags = document->options.color_flags;
	int y;

	for (y = 0; y < document->height; y++) {
		int x;

		for (x = 0; x < document->data[y].length; x++) {
			struct screen_char *schar = &document->data[y].ch.chars[x];

			set_term_color(schar, &colors, color_flags, color_mode);

			/* XXX: Entering hack zone! Change to clink color after
			 * link text has been recolored. */
			if (schar->data == ':' && colors.foreground == 0x0)
				colors.foreground = elformat.color.clink;
		}

		colors.foreground = 0x0;
	}
}

void *
html_special(struct html_context *html_context, html_special_type_T c, ...)
{
	va_list l;
	struct part *part;
	struct document *document;
	void *ret_val = NULL;

	assert(html_context);
	if_assert_failed return NULL;

	part = html_context->part;

	assert(part);
	if_assert_failed return NULL;

	document = part->document;

	va_start(l, c);
	switch (c) {
		case SP_TAG:
			if (document) {
				char *t = va_arg(l, char *);

				html_special_tag(document, t, X(part->cx), Y(part->cy));
			}
			break;
		case SP_FORM:
		{
			struct form *form = va_arg(l, struct form *);

			html_special_form(part, form);
			break;
		}
		case SP_CONTROL:
		{
			struct el_form_control *fc = va_arg(l, struct el_form_control *);

			html_special_form_control(part, fc);
			break;
		}
		case SP_TABLE:
			ret_val = renderer_context.convert_table;
			break;
		case SP_USED:
			ret_val = (void *) (intptr_t) !!document;
			break;
		case SP_CACHE_CONTROL:
		{
			struct cache_entry *cached = renderer_context.cached;

			cached->cache_mode = CACHE_MODE_NEVER;
			cached->expire = 0;
			break;
		}
		case SP_CACHE_EXPIRES:
		{
			time_t expires = va_arg(l, time_t);
			struct cache_entry *cached = renderer_context.cached;

			if (!expires || cached->cache_mode == CACHE_MODE_NEVER)
				break;

			timeval_from_seconds(&cached->max_age, expires);
			cached->expire = 1;
			break;
		}
		case SP_FRAMESET:
		{
			struct frameset_param *fsp = va_arg(l, struct frameset_param *);
			struct frameset_desc *frameset_desc;

			if (!fsp->parent && document->frame_desc)
				break;

			frameset_desc = create_frameset(fsp);
			if (!fsp->parent && !document->frame_desc)
				document->frame_desc = frameset_desc;

			ret_val = frameset_desc;
			break;
		}
		case SP_FRAME:
		{
			struct frameset_desc *parent = va_arg(l, struct frameset_desc *);
			char *name = va_arg(l, char *);
			char *url = va_arg(l, char *);

			add_frameset_entry(parent, NULL, name, url);
			break;
		}
		case SP_NOWRAP:
			renderer_context.nowrap = !!va_arg(l, int);
			break;
		case SP_REFRESH:
		{
			unsigned long seconds = va_arg(l, unsigned long);
			char *t = va_arg(l, char *);

			if (document) {
				if (document->refresh)
					done_document_refresh(document->refresh);
				document->refresh = init_document_refresh(t, seconds);
			}
			break;
		}
		case SP_COLOR_LINK_LINES:
			if (document && use_document_bg_colors(&document->options))
				color_link_lines(html_context);
			break;
		case SP_STYLESHEET:
#ifdef CONFIG_CSS
			if (document) {
				struct uri *uri = va_arg(l, struct uri *);

				add_to_uri_list(&document->css_imports, uri);
			}
#endif
			break;
		case SP_SCRIPT:
#ifdef CONFIG_ECMASCRIPT
			if (document) {
				struct uri *uri = va_arg(l, struct uri *);
/* LOG_JS("%s", uri->data); */
				add_to_uri_list(&document->ecmascript_imports, uri);
			}
#endif
			break;

		case SP_IFRAME:
		{
			if (document) {
				char *url = va_arg(l, char *);
				char *name = va_arg(l, char *);
				int y = va_arg(l, int);
				int width = va_arg(l, int);
				int height = va_arg(l, int);

				add_iframeset_entry(&document->iframe_desc, url, name, y, width, height, document->nlinks);
			}
			break;
		}
		default:
			break;

	}

	va_end(l);

	return ret_val;
}

void
free_table_cache(void)
{
	if (table_cache) {
		struct hash_item *item;
		int i;

		/* We do not free key here. */
		foreach_hash_item (item, *table_cache, i) {
			mem_free_if(item->value);
		}

		free_hash(&table_cache);
		table_cache_entries = 0;
	}
}

struct part *
format_html_part(struct html_context *html_context,
		 char *start, char *end,
		 int align, int margin, int width, struct document *document,
		 int x, int y, char *head,
		 int link_num)
{
	struct part *part;
	void *html_state;
	struct tag *saved_last_tag_to_move = renderer_context.last_tag_to_move;
	int saved_empty_format = renderer_context.empty_format;
	int saved_margin = html_context->margin;
	int saved_last_link_to_move = renderer_context.last_link_to_move;

	/* Hash creation if needed. */
	if (!table_cache) {
		table_cache = init_hash8();
	} else if (!document) {
		/* Search for cached entry. */
		struct table_cache_entry_key key;
		struct hash_item *item;

		/* Clear key to prevent potential alignment problem
		 * when keys are compared. */
		memset(&key, 0, sizeof(key));

		key.start = start;
		key.end = end;
		key.align = align;
		key.margin = margin;
		key.width = width;
		key.x = x;
		key.link_num = link_num;

		item = get_hash_item(table_cache,
				     (char *) &key,
				     sizeof(key));
		if (item) { /* We found it in cache, so just copy and return. */
			part = (struct part *)mem_alloc(sizeof(*part));
			if (part)  {
				copy_struct(part, &((struct table_cache_entry *)
						    item->value)->part);
				return part;
			}
		}
	}

	assertm(y >= 0, "format_html_part: y == %d", y);
	if_assert_failed return NULL;

	if (document) {
		struct node *node = (struct node *)mem_alloc(sizeof(*node));

		if (node) {
			int node_width = !html_context->table_level ? INT_MAX : width;

			set_box(&node->box, x, y, node_width, 1);
			add_to_list(document->nodes, node);
		}

		renderer_context.last_link_to_move = document->nlinks;
		renderer_context.last_tag_to_move = (struct tag *) &document->tags;
		renderer_context.last_tag_for_newline = (struct tag *) &document->tags;
	} else {
		renderer_context.last_link_to_move = 0;
		renderer_context.last_tag_to_move = (struct tag *) NULL;
		renderer_context.last_tag_for_newline = (struct tag *) NULL;
	}

	html_context->margin = margin;
	renderer_context.empty_format = !document;

	done_link_state_info();
	renderer_context.nobreak = 1;

	part = (struct part *)mem_calloc(1, sizeof(*part));
	if (!part) goto ret;

	part->document = document;
	part->box.x = x;
	part->box.y = y;
	part->cx = -1;
	part->cy = 0;
	part->link_num = link_num;

	html_state = init_html_parser_state(html_context, ELEMENT_IMMORTAL, align, margin, width);

	parse_html(start, end, part, head, html_context);

	done_html_parser_state(html_context, html_state);

	int_lower_bound(&part->max_width, part->box.width);

	renderer_context.nobreak = 0;

	done_link_state_info();
	mem_free_if(part->spaces);
#ifdef CONFIG_UTF8
	mem_free_if(part->char_width);
#endif

	if (document) {
		struct node *node = (struct node *)document->nodes.next;

		node->box.height = y - node->box.y + part->box.height;
	}

ret:
	renderer_context.last_link_to_move = saved_last_link_to_move;
	renderer_context.last_tag_to_move = saved_last_tag_to_move;
	renderer_context.empty_format = saved_empty_format;

	html_context->margin = saved_margin;

	if (html_context->table_level > 1 && !document
	    && table_cache
	    && table_cache_entries < MAX_TABLE_CACHE_ENTRIES) {
		/* Create a new entry. */
		/* Clear memory to prevent bad key comparaison due to alignment
		 * of key fields. */
		struct table_cache_entry *tce = (struct table_cache_entry *)mem_calloc(1, sizeof(*tce));

		if (tce) {
			tce->key.start = start;
			tce->key.end = end;
			tce->key.align = align;
			tce->key.margin = margin;
			tce->key.width = width;
			tce->key.x = x;
			tce->key.link_num = link_num;
			copy_struct(&tce->part, part);

			if (!add_hash_item(table_cache,
					   (char *) &tce->key,
					   sizeof(tce->key), tce)) {
				mem_free(tce);
			} else {
				table_cache_entries++;
			}
		}
	}

	return part;
}

void
render_html_document(struct cache_entry *cached, struct document *document,
		     struct string *buffer)
{
	struct html_context *html_context;
	struct part *part;
	char *start;
	char *end;
	struct string title;
	struct string head;
	int libdom;

	assert(cached && document);
	if_assert_failed return;

	if (!init_string(&head)) return;

	if (cached->head) add_to_string(&head, cached->head);

	start = buffer->source;
	end = buffer->source + buffer->length;

	html_context = init_html_parser(cached->uri, document,
	                                start, end, &head, &title,
	                                put_chars_conv, line_break,
	                                html_special);
	if (!html_context) return;

#ifdef CONFIG_LIBDOM
	libdom = !!document->dom;
#else
	libdom = 0;
#endif
	renderer_context.g_ctrl_num = 0;
	renderer_context.cached = cached;
	renderer_context.convert_table = get_convert_table(head.source,
							   document->options.cp,
							   libdom ? get_cp_index("utf-8") : document->options.assume_cp,
							   &document->cp,
							   &document->cp_status,
							   libdom ? 1 : document->options.hard_assume);
#ifdef CONFIG_UTF8
	html_context->options->utf8 = is_cp_utf8(document->options.cp);
#endif /* CONFIG_UTF8 */
	html_context->doc_cp = document->cp;

	if (title.length) {
		/* CSM_DEFAULT because init_html_parser() did not
		 * decode entities in the title.  */
		mem_free_set(&document->title, convert_string(renderer_context.convert_table,
						 title.source, title.length,
						 document->options.cp,
						 CSM_DEFAULT, NULL, NULL, NULL));
	}
	done_string(&title);

	part = format_html_part(html_context, start, end, par_elformat.align,
			        par_elformat.leftmargin + par_elformat.blockquote_level * (html_context->table_level == 0),
				document->options.document_width, document,
			        0, 0, head.source, 1);

	/* Drop empty allocated lines at end of document if any
	 * and adjust document height. */
	while (document->height && !document->data[document->height - 1].length)
		mem_free_if(document->data[--document->height].ch.chars);

	/* Calculate document width. */
	{
		int i;

		document->width = 0;
		for (i = 0; i < document->height; i++)
			int_lower_bound(&document->width, document->data[i].length);
	}

#if 1
	document->options.needs_width = 1;
#else
	/* FIXME: This needs more tuning since if we are centering stuff it
	 * does not work. */
	document->options.needs_width =
				(document->width + (document->options.margin
				 >= document->options.width));
#endif

	document->color.background = par_elformat.color.background;

	done_html_parser(html_context);

	/* Drop forms which has been serving as a placeholder for form items
	 * added in the wrong order due to the ordering of table rendering. */
	{
		struct form *form;

		foreach (form, document->forms) {
			if (form->form_num)
				continue;

			if (list_empty(form->items))
				done_form(form);

			break;
		}
	}

	/* @part was residing in html_context so it has to stay alive until
	 * done_html_parser(). */
	done_string(&head);
	mem_free_if(part);

#if 0 /* debug purpose */
	{
		FILE *f = fopen("forms", "ab");
		struct el_form_control *form;
		char *qq;
		fprintf(f,"FORM:\n");
		foreach (form, document->forms) {
			fprintf(f, "g=%d f=%d c=%d t:%d\n",
				form->g_ctrl_num, form->form_num,
				form->ctrl_num, form->type);
		}
		fprintf(f,"fragment: \n");
		for (qq = start; qq < end; qq++) fprintf(f, "%c", *qq);
		fprintf(f,"----------\n\n");
		fclose(f);
	}
#endif
}
