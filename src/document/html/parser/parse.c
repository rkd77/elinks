/* HTML core parser routines */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/css/apply.h"
#include "document/css/parser.h"
#include "document/html/parser/forms.h"
#include "document/html/parser/general.h"
#include "document/html/parser/link.h"
#include "document/html/parser/parse.h"
#include "document/html/parser/stack.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/options.h"
#include "intl/charsets.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/fastfind.h"
#include "util/memdebug.h"
#include "util/memory.h"
#include "util/string.h"

/* Unsafe macros */
#include "document/html/internal.h"


#define end_of_tag(c) ((c) == '>' || (c) == '<')

static inline int
atchr(register unsigned char c)
{
	return (c < 127 && (c > '>' || (c > ' ' && c != '=' && !end_of_tag(c))));
}

/* This function eats one html element. */
/* - e is pointer to the begining of the element (*e must be '<')
 * - eof is pointer to the end of scanned area
 * - parsed element name is stored in name, it's length is namelen
 * - first attribute is stored in attr
 * - end points to first character behind the html element */
/* It returns -1 when it failed (returned values in pointers are invalid) and
 * 0 for success. */
int
parse_element(register unsigned char *e, unsigned char *eof,
	      unsigned char **name, int *namelen,
	      unsigned char **attr, unsigned char **end)
{
#define next_char() if (++e == eof) return -1;

	assert(e && eof);
	if (e >= eof || *e != '<') return -1;

	next_char();
	if (name) *name = e;

	if (*e == '/') next_char();
	if (!isident(*e)) return -1;

	while (isident(*e)) next_char();

	if (!isspace(*e) && !end_of_tag(*e) && *e != '/' && *e != ':' && *e != '=')
		return -1;

	if (name && namelen) *namelen = e - *name;

	while (isspace(*e) || *e == '/' || *e == ':') next_char();

	/* Skip bad attribute */
	while (!atchr(*e) && !end_of_tag(*e) && !isspace(*e)) next_char();

	if (attr) *attr = e;

next_attr:
	while (isspace(*e)) next_char();

	/* Skip bad attribute */
	while (!atchr(*e) && !end_of_tag(*e) && !isspace(*e)) next_char();

	if (end_of_tag(*e)) goto end;

	while (atchr(*e)) next_char();
	while (isspace(*e)) next_char();

	if (*e != '=') {
		if (end_of_tag(*e)) goto end;
		goto next_attr;
	}
	next_char();

	while (isspace(*e)) next_char();

	if (isquote(*e)) {
		unsigned char quote = *e;

/* quoted_value: */
		next_char();
		while (*e != quote) next_char();
		next_char();
		/* The following apparently handles the case of <foo
		 * id="a""b">, however that is very rare and probably not
		 * conforming. More frequent (and mishandling it more fatal) is
		 * probably the typo of <foo id="a""> - we can handle it as
		 * long as this is commented out. --pasky */
		/* if (*e == quote) goto quoted_value; */
	} else {
		while (!isspace(*e) && !end_of_tag(*e)) next_char();
	}

	while (isspace(*e)) next_char();

	if (!end_of_tag(*e)) goto next_attr;

end:
	if (end) *end = e + (*e == '>');

	return 0;
}


#define realloc_chrs(x, l) mem_align_alloc(x, l, (l) + 1, 0xFF)

#define add_chr(s, l, c)						\
	do {								\
		if (!realloc_chrs(&(s), l)) return NULL;		\
		(s)[(l)++] = (c);					\
	} while (0)

unsigned char *
get_attr_value(register unsigned char *e, unsigned char *name,
	       int cp, enum html_attr_flags flags)
{
	unsigned char *n;
	unsigned char *name_start;
	unsigned char *attr = NULL;
	int attrlen = 0;
	int found;

next_attr:
	skip_space(e);
	if (end_of_tag(*e) || !atchr(*e)) goto parse_error;
	n = name;
	name_start = e;

	while (atchr(*n) && atchr(*e) && c_toupper(*e) == c_toupper(*n)) e++, n++;
	found = !*n && !atchr(*e);

	if (found && (flags & HTML_ATTR_TEST)) return name_start;

	while (atchr(*e)) e++;
	skip_space(e);
	if (*e != '=') {
		if (found) goto found_endattr;
		goto next_attr;
	}
	e++;
	skip_space(e);

	if (found) {
		if (!isquote(*e)) {
			while (!isspace(*e) && !end_of_tag(*e)) {
				if (!*e) goto parse_error;
				add_chr(attr, attrlen, *e);
				e++;
			}
		} else {
			unsigned char quote = *e;

/* parse_quoted_value: */
			while (*(++e) != quote) {
				if (!*e) goto parse_error;
				if (flags & HTML_ATTR_LITERAL_NL)
					add_chr(attr, attrlen, *e);
				else if (*e == ASCII_CR) continue;
				else if (*e != ASCII_TAB && *e != ASCII_LF)
					add_chr(attr, attrlen, *e);
				else if (!(flags & HTML_ATTR_EAT_NL))
					add_chr(attr, attrlen, ' ');
			}
			e++;
			/* The following apparently handles the case of <foo
			 * id="a""b">, however that is very rare and probably
			 * not conforming. More frequent (and mishandling it
			 * more fatal) is probably the typo of <foo id="a""> -
			 * we can handle it as long as this is commented out.
			 * --pasky */
#if 0
			if (*e == quote) {
				add_chr(attr, attrlen, *e);
				goto parse_quoted_value;
			}
#endif
		}

found_endattr:
		add_chr(attr, attrlen, '\0');
		attrlen--;

		if (/* Unused: !(flags & HTML_ATTR_NO_CONV) && */
		    memchr(attr, '&', attrlen)) {
			unsigned char *saved_attr = attr;

			attr = convert_string(NULL, saved_attr, attrlen, cp,
			                      CSM_QUERY, NULL, NULL, NULL);
			mem_free(saved_attr);
		}

		set_mem_comment(attr, name, strlen(name));
		return attr;

	} else {
		if (!isquote(*e)) {
			while (!isspace(*e) && !end_of_tag(*e)) {
				if (!*e) goto parse_error;
				e++;
			}
		} else {
			unsigned char quote = *e;

			do {
				while (*(++e) != quote)
					if (!*e) goto parse_error;
				e++;
			} while (/* See above. *e == quote */ 0);
		}
	}

	goto next_attr;

parse_error:
	mem_free_if(attr);
	return NULL;
}

#undef add_chr


/* Extract numerical value of attribute @name.
 * It will return a positive integer value on success,
 * or -1 on error. */
int
get_num(unsigned char *a, unsigned char *name, int cp)
{
	unsigned char *al = get_attr_val(a, name, cp);
	int result = -1;

	if (al) {
		unsigned char *end;
		long num;

		errno = 0;
		num = strtol(al, (char **) &end, 10);
		if (!errno && *al && !*end && num >= 0 && num <= INT_MAX)
			result = (int) num;

		mem_free(al);
	}

	return result;
}

/* Parse 'width[%],....'-like attribute @name of element @a.  If @limited is
 * set, it will limit the width value to the current usable width. Note that
 * @limited must be set to be able to parse percentage widths. */
/* The function returns width in characters or -1 in case of error. */
int
get_width(unsigned char *a, unsigned char *name, int limited,
          struct html_context *html_context)
{
	unsigned char *value = get_attr_val(a, name, html_context->doc_cp);
	unsigned char *str = value;
	unsigned char *end;
	int percentage = 0;
	int len;
	long width;

	if (!value) return -1;

	/* Skip spaces at start of string if any. */
	skip_space(str);

	/* Search for end of string or ',' character (ie. in "100,200") */
	for (len = 0; str[len] && str[len] != ','; len++);

	/* Go back, and skip spaces after width if any. */
	while (len && isspace(str[len - 1])) len--;
	if (!len) { mem_free(value); return -1; } /* Nothing to parse. */

	/* Is this a percentage ? */
	if (str[len - 1] == '%') len--, percentage = 1;

	/* Skip spaces between width number and percentage if any. */
	while (len && isspace(str[len - 1])) len--;
	if (!len) { mem_free(value); return -1; } /* Nothing to parse. */

	/* Shorten the string a bit, so strtoul() will work on useful
	 * part of it. */
	str[len] = '\0';

	/* Convert to number if possible. */
	errno = 0;
	width = strtoul((char *) str, (char **) &end, 10);

	/* @end points into the @value string so check @end position
	 * before freeing @value. */
	/* We will accept floats but ceil() them. */
	if (errno || (*end && *end != '.') || width >= INT_MAX) {
		/* Not a valid number. */
		mem_free(value);
		return -1;
	}

	mem_free(value);

#define WIDTH_PIXELS2CHARS(width) ((width) + (HTML_CHAR_WIDTH - 1) / 2) / HTML_CHAR_WIDTH;

	if (limited) {
		int maxwidth = get_html_max_width();

		if (percentage) {
			/* Value is a percentage. */
			width = width * maxwidth / 100;
		} else {
			/* Value is a number of pixels, makes an approximation. */
			width = WIDTH_PIXELS2CHARS(width);
		}

		if (width > maxwidth)
			width = maxwidth;

	} else {
		if (percentage) {
			/* No sense, we need @limited and @maxwidth for percentage. */
			return -1;
		} else {
			/* Value is a number of pixels, makes an approximation,
			 * no limit here */
			width = WIDTH_PIXELS2CHARS(width);
		}
	}

#undef WIDTH_PIXELS2CHARS

	if (width < 0)
		width = 0;

	return width;
}


unsigned char *
skip_comment(unsigned char *html, unsigned char *eof)
{
	if (html + 4 <= eof && html[2] == '-' && html[3] == '-') {
		html += 4;
		while (html < eof) {
			if (html + 2 <= eof && html[0] == '-' && html[1] == '-') {
				html += 2;
				while (html < eof && *html == '-') html++;
				while (html < eof && isspace(*html)) html++;
				if (html >= eof) return eof;
				if (*html == '>') return html + 1;
				continue;
			}
			html++;
		}

	} else {
		html += 2;
		while (html < eof) {
			if (html[0] == '>') return html + 1;
			html++;
		}
	}

	return eof;
}




enum element_type {
	ET_NESTABLE,
	ET_NON_NESTABLE,
	ET_NON_PAIRABLE,
	ET_LI,
};

struct element_info {
	/* Element name, uppercase. */
	unsigned char *name;

	/* Element handler. This does the relevant arguments processing and
	 * formatting (by calling renderer hooks). Note that in a few cases,
	 * this is just a placeholder and the element is given special care
	 * in start_element() (which is also where we call these handlers). */
	element_handler_T *open;

	element_handler_T *close;

	/* How many line-breaks to ensure we have before and after an element.
	 * Value of 1 means the element will be on a line on its own, value
	 * of 2 means that it will also have empty lines before and after.
	 * Note that this does not add up - it just ensures that there is
	 * at least so many linebreaks, but does not add more if that is the
	 * case. Therefore, something like e.g. </pre></p> will add only two
	 * linebreaks, not four. */
	/* In some stack killing logic, we use some weird heuristic based on
	 * whether an element is block or inline. That is determined from
	 * whether this attribute is zero on non-zero. */
	int linebreak;

	enum element_type type;
};

static struct element_info elements[] = {
 {"A",           html_a,           NULL,                 0, ET_NON_NESTABLE},
 {"ABBR",        html_italic,      NULL,                 0, ET_NESTABLE    },
 {"ADDRESS",     html_address,     NULL,                 2, ET_NESTABLE    },
 {"APPLET",      html_applet,      NULL,                 1, ET_NON_PAIRABLE},
 {"B",           html_bold,        NULL,                 0, ET_NESTABLE    },
 {"BASE",        html_base,        NULL,                 0, ET_NON_PAIRABLE},
 {"BASEFONT",    html_font,        NULL,                 0, ET_NON_PAIRABLE},
 {"BLOCKQUOTE",  html_blockquote,  NULL,                 2, ET_NESTABLE    },
 {"BODY",        html_body,        NULL,                 0, ET_NESTABLE    },
 {"BR",          html_br,          NULL,                 1, ET_NON_PAIRABLE},
 {"BUTTON",      html_button,      NULL,                 0, ET_NESTABLE    },
 {"CAPTION",     html_center,      NULL,                 1, ET_NESTABLE    },
 {"CENTER",      html_center,      NULL,                 1, ET_NESTABLE    },
 {"CODE",        html_fixed,       NULL,                 0, ET_NESTABLE    },
 {"DD",          html_dd,          NULL,                 1, ET_NON_PAIRABLE},
 {"DFN",         html_bold,        NULL,                 0, ET_NESTABLE    },
 {"DIR",         html_ul,          NULL,                 2, ET_NESTABLE    },
 {"DIV",         html_linebrk,     NULL,                 1, ET_NESTABLE    },
 {"DL",          html_dl,          NULL,                 2, ET_NESTABLE    },
 {"DT",          html_dt,          NULL,                 1, ET_NON_PAIRABLE},
 {"EM",          html_italic,      NULL,                 0, ET_NESTABLE    },
 {"EMBED",       html_embed,       NULL,                 0, ET_NON_PAIRABLE},
 {"FIXED",       html_fixed,       NULL,                 0, ET_NESTABLE    },
 {"FONT",        html_font,        NULL,                 0, ET_NESTABLE    },
 {"FORM",        html_form,        NULL,                 1, ET_NESTABLE    },
 {"FRAME",       html_frame,       NULL,                 1, ET_NON_PAIRABLE},
 {"FRAMESET",    html_frameset,    NULL,                 1, ET_NESTABLE    },
 {"H1",          html_h1,          NULL,                 2, ET_NON_NESTABLE},
 {"H2",          html_h2,          NULL,                 2, ET_NON_NESTABLE},
 {"H3",          html_h3,          NULL,                 2, ET_NON_NESTABLE},
 {"H4",          html_h4,          NULL,                 2, ET_NON_NESTABLE},
 {"H5",          html_h5,          NULL,                 2, ET_NON_NESTABLE},
 {"H6",          html_h6,          NULL,                 2, ET_NON_NESTABLE},
 {"HEAD",        html_head,        NULL,                 0, ET_NESTABLE    },
 {"HR",          html_hr,          NULL,                 2, ET_NON_PAIRABLE},
 {"HTML",        html_html,        html_html_close,      0, ET_NESTABLE    },
 {"I",           html_italic,      NULL,                 0, ET_NESTABLE    },
 {"IFRAME",      html_iframe,      NULL,                 1, ET_NON_PAIRABLE},
 {"IMG",         html_img,         NULL,                 0, ET_NON_PAIRABLE},
 {"INPUT",       html_input,       NULL,                 0, ET_NON_PAIRABLE},
 {"LI",          html_li,          NULL,                 1, ET_LI          },
 {"LINK",        html_link,        NULL,                 1, ET_NON_PAIRABLE},
 {"LISTING",     html_pre,         NULL,                 2, ET_NESTABLE    },
 {"MENU",        html_ul,          NULL,                 2, ET_NESTABLE    },
 {"META",        html_meta,        NULL,                 0, ET_NON_PAIRABLE},
 {"NOFRAMES",    html_noframes,    NULL,                 0, ET_NESTABLE    },
 {"NOSCRIPT",    html_noscript,    NULL,                 0, ET_NESTABLE    },
 {"OBJECT",      html_object,      NULL,                 1, ET_NON_PAIRABLE},
 {"OL",          html_ol,          NULL,                 2, ET_NESTABLE    },
 {"OPTION",      html_option,      NULL,                 1, ET_NON_PAIRABLE},
 {"P",           html_p,           NULL,                 2, ET_NON_NESTABLE},
 {"PRE",         html_pre,         NULL,                 2, ET_NESTABLE    },
 {"Q",           html_quote,       html_quote_close,     0, ET_NESTABLE    },
 {"S",           html_underline,   NULL,                 0, ET_NESTABLE    },
 {"SCRIPT",      html_script,      NULL,                 0, ET_NESTABLE    },
 {"SELECT",      html_select,      NULL,                 0, ET_NESTABLE    },
 {"SPAN",        html_span,        NULL,                 0, ET_NESTABLE    },
 {"STRIKE",      html_underline,   NULL,                 0, ET_NESTABLE    },
 {"STRONG",      html_bold,        NULL,                 0, ET_NESTABLE    },
 {"STYLE",       html_style,       html_style_close,     0, ET_NESTABLE    },
 {"SUB",         html_subscript,   html_subscript_close, 0, ET_NESTABLE    },
 {"SUP",         html_superscript, NULL,                 0, ET_NESTABLE    },
 {"TABLE",       html_table,       NULL,                 2, ET_NESTABLE    },
 {"TD",          html_td,          NULL,                 0, ET_NESTABLE    },
 {"TEXTAREA",    html_textarea,    NULL,                 0, ET_NON_PAIRABLE},
 {"TH",          html_th,          NULL,                 0, ET_NESTABLE    },
 {"TITLE",       html_title,       NULL,                 0, ET_NESTABLE    },
 {"TR",          html_tr,          NULL,                 1, ET_NESTABLE    },
 {"TT",          html_tt,          NULL,                 0, ET_NON_NESTABLE},
 {"U",           html_underline,   NULL,                 0, ET_NESTABLE    },
 {"UL",          html_ul,          NULL,                 2, ET_NESTABLE    },
 {"XMP",         html_xmp,         html_xmp_close,       2, ET_NESTABLE    },
 {NULL,          NULL,             NULL,                 0, ET_NESTABLE    },
};

#define NUMBER_OF_TAGS (sizeof_array(elements) - 1)


#ifndef USE_FASTFIND

static int
compar(const void *a, const void *b)
{
	return c_strcasecmp(((struct element_info *) a)->name,
			    ((struct element_info *) b)->name);
}

#else

static struct element_info *internal_pointer;

/* Reset internal list pointer */
static void
tags_list_reset(void)
{
	internal_pointer = elements;
}

/* Returns a pointer to a struct that contains
 * current key and data pointers and increment
 * internal pointer.
 * It returns NULL when key is NULL. */
static struct fastfind_key_value *
tags_list_next(void)
{
	static struct fastfind_key_value kv;

	if (!internal_pointer->name) return NULL;

	kv.key = internal_pointer->name;
	kv.data = internal_pointer;

	internal_pointer++;

	return &kv;
}

static struct fastfind_index ff_tags_index
	= INIT_FASTFIND_INDEX("tags_lookup", tags_list_reset, tags_list_next);

#endif /* USE_FASTFIND */


void
init_tags_lookup(void)
{
#ifdef USE_FASTFIND
	fastfind_index(&ff_tags_index, FF_COMPRESS | FF_LOCALE_INDEP);
#endif
}

void
free_tags_lookup(void)
{
#ifdef USE_FASTFIND
	fastfind_done(&ff_tags_index);
#endif
}


static unsigned char *process_element(unsigned char *name, int namelen, int endingtag,
                unsigned char *html, unsigned char *prev_html,
                unsigned char *eof, unsigned char *attr,
                struct html_context *html_context);

/* Count the consecutive newline entity references (e.g. "&#13;") at
 * the beginning of the range from @html to @eof.  Store the number of
 * newlines to *@newlines_out and return the address where they end.
 *
 * This function currently requires a semicolon at the end of any
 * entity reference, and does not support U+2028 LINE SEPARATOR and
 * U+2029 PARAGRAPH SEPARATOR.  */
static const unsigned char *
count_newline_entities(const unsigned char *html, const unsigned char *eof,
		       int *newlines_out)
{
	int newlines = 0;
	int prev_was_cr = 0; /* treat CRLF as one newline, not two */

	while ((html + 5 < eof && html[0] == '&' && html[1] == '#')) {
		const unsigned char *peek = html + 2;
		int this_is_cr;

		if (*peek == 'x' || *peek == 'X') {
			++peek;
			while (peek < eof && *peek == '0')
				++peek;
			if (peek == eof)
				break;
			else if (*peek == 'a' || *peek == 'A')
				this_is_cr = 0;
			else if (*peek == 'd' || *peek == 'D')
				this_is_cr = 1;
			else
				break;
			++peek;
		} else {
			while (peek < eof && *peek == '0')
				++peek;
			if (eof - peek < 2 || *peek != '1')
				break;
			else if (peek[1] == '0')
				this_is_cr = 0;
			else if (peek[1] == '3')
				this_is_cr = 1;
			else
				break;
			peek += 2;
		}
		/* @peek should now be pointing to the semicolon of
		 * e.g. "&#00013;" or "&#x00a;".  Or more digits might
		 * follow.  */
		if (peek == eof || *peek != ';')
			break;
		++peek;

		if (this_is_cr || !prev_was_cr)
			++newlines;
		prev_was_cr = this_is_cr;
		html = peek;
	}

	*newlines_out = newlines;
	return html;
}

void
parse_html(unsigned char *html, unsigned char *eof,
	   struct part *part, unsigned char *head,
	   struct html_context *html_context)
{
	unsigned char *base_pos = html;
	int noupdate = 0;

	html_context->putsp = HTML_SPACE_SUPPRESS;
	html_context->line_breax = html_context->table_level ? 2 : 1;
	html_context->position = 0;
	html_context->was_br = 0;
	html_context->was_li = 0;
	html_context->was_body = 0;
/*	html_context->was_body_background = 0; */
	html_context->part = part;
	html_context->eoff = eof;
	if (head) process_head(html_context, head);

main_loop:
	while (html < eof) {
		unsigned char *name, *attr, *end;
		int namelen, endingtag;
		int dotcounter = 0;

		if (!noupdate) {
			html_context->part = part;
			html_context->eoff = eof;
			base_pos = html;
		} else {
			noupdate = 0;
		}

		if (isspace(*html) && !html_is_preformatted()) {
			unsigned char *h = html;

			while (h < eof && isspace(*h))
				h++;
			if (h + 1 < eof && h[0] == '<' && h[1] == '/') {
				if (!parse_element(h, eof, &name, &namelen, &attr, &end)) {
					put_chrs(html_context, base_pos, html - base_pos);
					base_pos = html = h;
					html_context->putsp = HTML_SPACE_ADD;
					goto element;
				}
			}
			html++;
			if (!(html_context->position + (html - base_pos - 1)))
				goto skip_w; /* ??? */
			if (*(html - 1) == ' ') {	/* Do not replace with isspace() ! --Zas */
				/* BIG performance win; not sure if it doesn't cause any bug */
				if (html < eof && !isspace(*html)) {
					noupdate = 1;
					continue;
				}
				put_chrs(html_context, base_pos, html - base_pos);
			} else {
				put_chrs(html_context, base_pos, html - base_pos - 1);
				put_chrs(html_context, " ", 1);
			}

skip_w:
			while (html < eof && isspace(*html))
				html++;
			continue;
		}

		if (html_is_preformatted()) {
			html_context->putsp = HTML_SPACE_NORMAL;
			if (*html == ASCII_TAB) {
				put_chrs(html_context, base_pos, html - base_pos);
				put_chrs(html_context, "        ",
				         8 - (html_context->position % 8));
				html++;
				continue;

			} else if (*html == ASCII_CR || *html == ASCII_LF) {
				put_chrs(html_context, base_pos, html - base_pos);
				if (html - base_pos == 0 && html_context->line_breax > 0)
					html_context->line_breax--;
next_break:
				if (*html == ASCII_CR && html < eof - 1
				    && html[1] == ASCII_LF)
					html++;
				ln_break(html_context, 1);
				html++;
				if (*html == ASCII_CR || *html == ASCII_LF) {
					html_context->line_breax = 0;
					goto next_break;
				}
				continue;

			} else if (html + 5 < eof && *html == '&') {
				/* Really nasty hack to make &#13; handling in
				 * <pre>-tags lynx-compatible. It works around
				 * the entity handling done in the renderer,
				 * since checking #13 value there would require
				 * something along the lines of NBSP_CHAR or
				 * checking for '\n's in AT_PREFORMATTED text. */
				/* See bug 52 and 387 for more info. */
				int length = html - base_pos;
				int newlines;

				html = (unsigned char *) count_newline_entities(html, eof, &newlines);
				if (newlines) {
					put_chrs(html_context, base_pos, length);
					ln_break(html_context, newlines);
					continue;
				}
			}
		}

		while (*html < ' ') {
			if (html - base_pos)
				put_chrs(html_context, base_pos, html - base_pos);

			dotcounter++;
			base_pos = ++html;
			if (*html >= ' ' || isspace(*html) || html >= eof) {
				unsigned char *dots = fmem_alloc(dotcounter);

				if (dots) {
					memset(dots, '.', dotcounter);
					put_chrs(html_context, dots, dotcounter);
					fmem_free(dots);
				}
				goto main_loop;
			}
		}

		if (html + 2 <= eof && html[0] == '<' && (html[1] == '!' || html[1] == '?')
		    && !(html_context->was_xmp || html_context->was_style)) {
			put_chrs(html_context, base_pos, html - base_pos);
			html = skip_comment(html, eof);
			continue;
		}

		if (*html != '<' || parse_element(html, eof, &name, &namelen, &attr, &end)) {
			html++;
			noupdate = 1;
			continue;
		}

element:
		endingtag = *name == '/'; name += endingtag; namelen -= endingtag;
		if (!endingtag && html_context->putsp == HTML_SPACE_ADD && !html_top->invisible)
			put_chrs(html_context, " ", 1);
		put_chrs(html_context, base_pos, html - base_pos);
		if (!html_is_preformatted() && !endingtag && html_context->putsp == HTML_SPACE_NORMAL) {
			unsigned char *ee = end;
			unsigned char *nm;

			while (!parse_element(ee, eof, &nm, NULL, NULL, &ee))
				if (*nm == '/')
					goto ng;
			if (ee < eof && isspace(*ee)) {
				put_chrs(html_context, " ", 1);
			}
		}
ng:
		html = process_element(name, namelen, endingtag, end, html, eof, attr, html_context);
	}

	if (noupdate) put_chrs(html_context, base_pos, html - base_pos);
	ln_break(html_context, 1);
	/* Restore the part in case the html_context was trashed in the last
	 * iteration so that when destroying the stack in the caller we still
	 * get the right part pointer. */
	html_context->part = part;
	html_context->putsp = HTML_SPACE_SUPPRESS;
	html_context->position = 0;
	html_context->was_br = 0;
}

static unsigned char *
start_element(struct element_info *ei,
              unsigned char *name, int namelen,
              unsigned char *html,
              unsigned char *eof, unsigned char *attr,
              struct html_context *html_context)
{
#define ELEMENT_RENDER_PROLOGUE \
	ln_break(html_context, ei->linebreak); \
	a = get_attr_val(attr, "id", html_context->doc_cp); \
	if (a) { \
		html_context->special_f(html_context, SP_TAG, a); \
		mem_free(a); \
	}

	unsigned char *a;
	struct par_attrib old_format;
	int restore_format;
#ifdef CONFIG_CSS
	struct css_selector *selector = NULL;
#endif

	if (html_top->type == ELEMENT_WEAK) {
		pop_html_element(html_context);
	}

	/* We try to process nested <script> if we didn't process the parent
	 * one. */
	if (html_top->invisible
	    && (ei->open != html_script || html_top->invisible < 2)) {
		ELEMENT_RENDER_PROLOGUE
		return html;
	}

	restore_format = html_is_preformatted();
	old_format = par_format;

	/* Support for <meta refresh="..."> inside <body>. (bug 700) */
	if (ei->open == html_meta && html_context->was_body) {
		html_handle_body_meta(html_context, name - 1, eof);
		html_context->was_body = 0;
	}

#ifdef CONFIG_CSS
	if (ei->open == html_style && html_context->options->css_enable) {
		css_parse_stylesheet(&html_context->css_styles,
				     html_context->base_href, html, eof);
	}
#endif

	if (ei->type == ET_NON_NESTABLE || ei->type == ET_LI) {
		struct html_element *e;

		if (ei->type == ET_NON_NESTABLE) {
			foreach (e, html_context->stack) {
				if (e->type < ELEMENT_KILLABLE) break;
				if (is_block_element(e) || is_inline_element(ei)) break;
			}
		} else {
			foreach (e, html_context->stack) {
				if (is_block_element(e) && is_inline_element(ei)) break;
				if (e->type < ELEMENT_KILLABLE) break;
				if (!c_strlcasecmp(e->name, e->namelen, name, namelen)) break;
			}
		}

		if (!c_strlcasecmp(e->name, e->namelen, name, namelen)) {
			while (e->prev != (void *) &html_context->stack)
				kill_html_stack_item(html_context, e->prev);

			if (e->type > ELEMENT_IMMORTAL)
				kill_html_stack_item(html_context, e);
		}
	}

	if (ei->type != ET_NON_PAIRABLE) {
		html_stack_dup(html_context, ELEMENT_KILLABLE);
		html_top->name = name;
		html_top->namelen = namelen;
		html_top->options = attr;
		html_top->linebreak = ei->linebreak;

#ifdef CONFIG_ECMASCRIPT
		if (has_attr(attr, "onClick", html_context->doc_cp)) {
			/* XXX: Put something better to format.link. --pasky */
			mem_free_set(&format.link, stracpy("javascript:void(0);"));
			mem_free_set(&format.target, stracpy(html_context->base_target));
			format.style.fg = format.clink;
			html_top->pseudo_class = ELEMENT_LINK;
			mem_free_set(&format.title, stracpy("onClick placeholder"));
			/* Er. I know. Well, double html_focusable()s shouldn't
			 * really hurt. */
			html_focusable(html_context, attr);
		}
#endif
	}

#ifdef CONFIG_CSS
	if (html_top->options && html_context->options->css_enable) {
		/* XXX: We should apply CSS otherwise as well, but that'll need
		 * some deeper changes in order to have options filled etc.
		 * Probably just applying CSS from more places, since we
		 * usually have type != ET_NESTABLE when we either (1)
		 * rescan on your own from somewhere else (2) html_stack_dup()
		 * in our own way.  --pasky */
		mem_free_set(&html_top->attr.id,
			     get_attr_val(attr, "id", html_context->doc_cp));
		mem_free_set(&html_top->attr.class,
			     get_attr_val(attr, "class", html_context->doc_cp));
		/* Call it now to gain some of the stuff which might affect
		 * formatting of some elements. */
		/* FIXME: The caching of the CSS selector is broken, since t can
		 * lead to wrong styles being applied to following elements, so
		 * disabled for now. */
		selector = get_css_selector_for_element(html_context, html_top,
							&html_context->css_styles,
							&html_context->stack);

		if (selector) {
			apply_css_selector_style(html_context, html_top, selector);
			done_css_selector(selector);
		}
	}
	/* Now this was the reason for this whole funny ELEMENT_RENDER_PROLOGUE
	 * bussiness. Only now we have the definitive linebreak value, since
	 * that's what the display: property plays with. */
#endif
	ELEMENT_RENDER_PROLOGUE
	if (ei->open) ei->open(html_context, attr, html, eof, &html);
#ifdef CONFIG_CSS
	if (selector && html_top->options) {
		/* Call it now to override default colors of the elements. */
		selector = get_css_selector_for_element(html_context, html_top,
							&html_context->css_styles,
							&html_context->stack);

		if (selector) {
			apply_css_selector_style(html_context, html_top, selector);
			done_css_selector(selector);
		}
	}
#endif

	if (ei->open != html_br) html_context->was_br = 0;

	if (restore_format) par_format = old_format;

	return html;
#undef ELEMENT_RENDER_PROLOGUE
}

static unsigned char *
end_element(struct element_info *ei,
            unsigned char *name, int namelen,
            unsigned char *html,
            unsigned char *eof, unsigned char *attr,
            struct html_context *html_context)
{
	struct html_element *e, *elt;
	int lnb = 0;
	int kill = 0;

	html_context->was_br = 0;
	if (ei->type == ET_NON_PAIRABLE || ei->type == ET_LI)
		return html;

	if (ei->close) ei->close(html_context, attr, html, eof, &html);

	/* dump_html_stack(html_context); */
	foreach (e, html_context->stack) {
		if (is_block_element(e) && is_inline_element(ei)) kill = 1;
		if (c_strlcasecmp(e->name, e->namelen, name, namelen)) {
			if (e->type < ELEMENT_KILLABLE)
				break;
			else
				continue;
		}
		if (kill) {
			kill_html_stack_item(html_context, e);
			break;
		}
		for (elt = e;
		     elt != (void *) &html_context->stack;
		     elt = elt->prev)
			if (elt->linebreak > lnb)
				lnb = elt->linebreak;

		/* This hack forces a line break after a list end. It is needed
		 * when ending a list with the last <li> having no text the
		 * line_breax is 2 so the ending list's linebreak will be
		 * ignored when calling ln_break(). */
		if (html_context->was_li)
			html_context->line_breax = 0;

		ln_break(html_context, lnb);
		while (e->prev != (void *) &html_context->stack)
			kill_html_stack_item(html_context, e->prev);
		kill_html_stack_item(html_context, e);
		break;
	}
	/* dump_html_stack(html_context); */

	return html;
}

static unsigned char *
process_element(unsigned char *name, int namelen, int endingtag,
                unsigned char *html, unsigned char *prev_html,
                unsigned char *eof, unsigned char *attr,
                struct html_context *html_context)

{
	struct element_info *ei;

#ifndef USE_FASTFIND
	{
		struct element_info elem;
		unsigned char tmp;

		tmp = name[namelen];
		name[namelen] = '\0';

		elem.name = name;
		ei = bsearch(&elem, elements, NUMBER_OF_TAGS, sizeof(elem), compar);
		name[namelen] = tmp;
	}
#else
	ei = (struct element_info *) fastfind_search(&ff_tags_index, name, namelen);
#endif
	if (html_context->was_xmp || html_context->was_style) {
		if (!ei || (ei->open != html_xmp && ei->open != html_style) || !endingtag) {
			put_chrs(html_context, "<", 1);
			return prev_html + 1;
		}
	}

	if (!ei) return html;

	if (!endingtag) {
		return start_element(ei, name, namelen, html, eof, attr, html_context);
	} else {
		return end_element(ei, name, namelen, html, eof, attr, html_context);
	}
}

void
scan_http_equiv(unsigned char *s, unsigned char *eof, struct string *head,
		struct string *title, struct document_options *options)
{
	unsigned char *name, *attr, *he, *c;
	int namelen;

	if (title && !init_string(title)) return;

	add_char_to_string(head, '\n');

se:
	while (s < eof && *s != '<') {
sp:
		s++;
	}
	if (s >= eof) return;
	if (s + 2 <= eof && (s[1] == '!' || s[1] == '?')) {
		s = skip_comment(s, eof);
		goto se;
	}
	if (parse_element(s, eof, &name, &namelen, &attr, &s)) goto sp;

ps:
	if (!c_strlcasecmp(name, namelen, "HEAD", 4)) goto se;
	if (!c_strlcasecmp(name, namelen, "/HEAD", 5)) return;
	if (!c_strlcasecmp(name, namelen, "BODY", 4)) return;
	if (title && !title->length && !c_strlcasecmp(name, namelen, "TITLE", 5)) {
		unsigned char *s1;

xse:
		s1 = s;
		while (s < eof && *s != '<') {
xsp:
			s++;
		}
		if (s - s1)
			add_bytes_to_string(title, s1, s - s1);
		if (s >= eof) goto se;
		if (s + 2 <= eof && (s[1] == '!' || s[1] == '?')) {
			s = skip_comment(s, eof);
			goto xse;
		}
		if (parse_element(s, eof, &name, &namelen, &attr, &s)) {
			s1 = s;
			goto xsp;
		}
		clr_spaces(title->source);
		goto ps;
	}
	if (c_strlcasecmp(name, namelen, "META", 4)) goto se;

	/* FIXME (bug 784): options->cp is the terminal charset;
	 * should use the document charset instead.  */
	he = get_attr_val(attr, "charset", options->cp);
	if (he) {
		add_to_string(head, "Charset: ");
		add_to_string(head, he);
		mem_free(he);
	}

	/* FIXME (bug 784): options->cp is the terminal charset;
	 * should use the document charset instead.  */
	he = get_attr_val(attr, "http-equiv", options->cp);
	if (!he) goto se;

	add_to_string(head, he);
	mem_free(he);

	/* FIXME (bug 784): options->cp is the terminal charset;
	 * should use the document charset instead.  */
	c = get_attr_val(attr, "content", options->cp);
	if (c) {
		add_to_string(head, ": ");
		add_to_string(head, c);
	        mem_free(c);
	}

	add_crlf_to_string(head);
	goto se;
}
