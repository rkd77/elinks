/* HTML core parser routines */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>

#include "elinks.h"

#include "config/options.h"
#include "document/css/apply.h"
#include "document/css/css.h"
#include "document/css/parser.h"
#include "document/html/parser/forms.h"
#include "document/html/parser/general.h"
#include "document/html/parser/link.h"
#include "document/html/parser/parse.h"
#include "document/html/parser/stack.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/libdom/tags.h"
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

static int
tag_is_inline_element(int tag)
{
	return tags_dom_html_array[tag].linebreak == 0;
}

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
parse_element(register char *e, char *eof,
	      char **name, int *namelen,
	      char **attr, char **end)
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

char *
get_attr_value(register char *e, char *name,
	       int cp, enum html_attr_flags flags)
{
	char *n;
	char *name_start;
	char *attr = NULL;
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
			char *saved_attr = attr;

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

int get_num2(char *al)
{
	int result = -1;
	
	if (al) {
		char *end;
		long num;

		errno = 0;
		num = strtol(al, (char **) &end, 10);
		if (!errno && *al && !*end && num >= 0 && num <= INT_MAX)
			result = (int) num;

		mem_free(al);
	}

	return result;
}
/* Extract numerical value of attribute @name.
 * It will return a positive integer value on success,
 * or -1 on error. */
int
get_num(char *a, char *name, int cp)
{
	char *al = get_attr_val(a, name, cp);

	return get_num2(al);
}

int
get_width2(struct html_context *html_context, char *value, int limited)
{
	char *str = value;
	char *end;
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

/* Parse 'width[%],....'-like attribute @name of element @a.  If @limited is
 * set, it will limit the width value to the current usable width. Note that
 * @limited must be set to be able to parse percentage widths. */
/* The function returns width in characters or -1 in case of error. */
int
get_width(char *a, char *name, int limited,
          struct html_context *html_context)
{
	char *value = get_attr_val(a, name, html_context->doc_cp);

	return get_width2(html_context, value, limited);
}

char *
skip_comment(char *html, char *eof)
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

//enum element_type {
//	ET_NESTABLE,
//	ET_NON_NESTABLE,
//	ET_NON_PAIRABLE,
//	ET_LI,
//};

struct element_info {
	/* Element name, uppercase. */
	char *name;

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
 {"AUDIO",       html_audio,       NULL,                 1, ET_NON_PAIRABLE},
 {"B",           html_bold,        NULL,                 0, ET_NESTABLE    },
 {"BASE",        html_base,        NULL,                 0, ET_NON_PAIRABLE},
 {"BASEFONT",    html_font,        NULL,                 0, ET_NON_PAIRABLE},
 {"BLOCKQUOTE",  html_blockquote,  html_blockquote_close,2, ET_NESTABLE    },
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
 {"SOURCE",      html_source,      NULL,                 1, ET_NON_PAIRABLE},
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
 {"VIDEO",       html_video,       NULL,                 1, ET_NON_PAIRABLE},
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


static char *process_element(char *name, int namelen, int endingtag,
                char *html, char *prev_html,
                char *eof, char *attr,
                struct html_context *html_context);

/* Count the consecutive newline entity references (e.g. "&#13;") at
 * the beginning of the range from @html to @eof.  Store the number of
 * newlines to *@newlines_out and return the address where they end.
 *
 * This function currently requires a semicolon at the end of any
 * entity reference, and does not support U+2028 LINE SEPARATOR and
 * U+2029 PARAGRAPH SEPARATOR.  */
static const char *
count_newline_entities(const char *html, const char *eof,
		       int *newlines_out)
{
	int newlines = 0;
	int prev_was_cr = 0; /* treat CRLF as one newline, not two */

	while ((html + 5 < eof && html[0] == '&' && html[1] == '#')) {
		const char *peek = html + 2;
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

static bool
dump_dom_element(struct source_renderer *renderer, dom_node *node, int depth)
{
	dom_exception exc;
	dom_string *node_name = NULL;
	dom_string *id_value = NULL;
	dom_string *onclick_value = NULL;
	dom_node_type type;
	dom_html_element_type html_type;
	struct html_context *html_context = renderer->html_context;

	/* Only interested in element nodes */
	exc = dom_node_get_node_type(node, &type);

	if (DOM_NO_ERR != exc) {
		DBG("Exception raised for node_get_node_type\n");
		return false;
	} else {
		if (DOM_TEXT_NODE == type) {
			dom_string *str = NULL;

//fprintf(stderr, "skip_html=%d skip_textarea=%d skip->select=%d\n", html_context->skip_html, html_context->skip_textarea, html_context->skip_select);
			if (html_context->skip_textarea || html_context->skip_select || (html_context->was_style && !html_context->support_css)) {
				return true;
			}
			exc = dom_node_get_text_content(node, &str);

			if (exc == DOM_NO_ERR && str) {
				int length = dom_string_byte_length(str);
				unsigned char *html = (unsigned char *)dom_string_data(str);
				unsigned char *eof = html + length;
				unsigned char *base_pos = html;

				if (length > 0 && (eof[-1] == '\n')) {
					length--;
					eof--;
				}
#ifdef CONFIG_CSS
				if (html_context->was_style) {
					css_parse_stylesheet(&html_context->css_styles, html_context->base_href, html, eof);
					dom_string_unref(str);

					return true;
				}
#endif
//fprintf(stderr, "html='%s'\n", html);
				if (length > 0) {
					int noupdate = 0;
main_loop:
					if (!html_is_preformatted()) {
						unsigned char *buffer = fmem_alloc(length+1);
						unsigned char *dst;
						html_context->part = renderer->part;
						html_context->eoff = eof;

						if (!buffer) {
							dom_string_unref(str);
							return false;
						}
						dst = buffer;
						*dst = *html;
						for (html++; html <= eof; html++) {
							if (isspace(*html)) {
								if (*dst != ' ') {
									*(++dst) = ' ';
								}
							} else {
								*(++dst) = *html;
							}
						}
						if (dst - buffer > 1) {
							if (*buffer == ' ') {
								html_context->putsp = HTML_SPACE_ADD;
							}
						}
						put_chrs(html_context, buffer, dst - buffer);
						fmem_free(buffer);
					}

					while (html < eof) {
						int dotcounter = 0;

						if (!noupdate) {
							html_context->part = renderer->part;
							html_context->eoff = eof;
							base_pos = html;
						} else {
							noupdate = 0;
						}

///						if (isspace(*html) && !html_is_preformatted()) {
//							unsigned char *h = html;

//							while (h < eof && isspace(*h))
//								h++;
//			if (h + 1 < eof && h[0] == '<' && h[1] == '/') {
//				if (!parse_element(h, eof, &name, &namelen, &attr, &end)) {
//					put_chrs(html_context, base_pos, html - base_pos);
//					base_pos = html = h;
//					html_context->putsp = HTML_SPACE_ADD;
//					goto element;
//				}
//			}
///							html++;
///							if (!(html_context->position + (html - base_pos - 1)))
///								goto skip_w; /* ??? */
///							if (*(html - 1) == ' ') {	/* Do not replace with isspace() ! --Zas */
								/* BIG performance win; not sure if it doesn't cause any bug */
///								if (html < eof && !isspace(*html)) {
///									noupdate = 1;
///									continue;
///								}
///								put_chrs(html_context, base_pos, html - base_pos);
///							} else {
///								put_chrs(html_context, base_pos, html - base_pos - 1);
///								put_chrs(html_context, " ", 1);
///							}
///skip_w:
//							while (html < eof && isspace(*html))
//								html++;
///							continue;
///						}

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

//		if (html + 2 <= eof && html[0] == '<' && (html[1] == '!' || html[1] == '?')
//		    && !(html_context->was_xmp || html_context->was_style)) {
//			put_chrs(html_context, base_pos, html - base_pos);
//			html = skip_comment(html, eof);
//			continue;
//		}

//		if (*html != '<' || parse_element(html, eof, &name, &namelen, &attr, &end)) {
						html++;
						noupdate = 1;
						continue;
					}
//		}

//element:
//		endingtag = *name == '/'; name += endingtag; namelen -= endingtag;
//		if (!endingtag && html_context->putsp == HTML_SPACE_ADD && !html_top->invisible)
//			put_chrs(html_context, " ", 1);
//		put_chrs(html_context, base_pos, html - base_pos);
//		if (!html_is_preformatted() && !endingtag && html_context->putsp == HTML_SPACE_NORMAL) {
//			unsigned char *ee = end;
//			unsigned char *nm;

//			while (!parse_element(ee, eof, &nm, NULL, NULL, &ee))
//				if (*nm == '/')
//					goto ng;
//			if (ee < eof && isspace(*ee)) {
//				put_chrs(html_context, " ", 1);
//			}
//		}
//ng:
//		html = process_element(name, namelen, endingtag, end, html, eof, attr, html_context);

					if (noupdate) put_chrs(html_context, base_pos, html - base_pos);
					//ln_break(html_context, 1);
					/* Restore the part in case the html_context was trashed in the last
					* iteration so that when destroying the stack in the caller we still
					* get the right part pointer. */
					html_context->part = renderer->part;
					html_context->putsp = HTML_SPACE_SUPPRESS;
					html_context->position = 0;
					html_context->was_br = 0;
				}
				dom_string_unref(str);
			}
			return true;
		}
		if (type != DOM_ELEMENT_NODE) {
			/* Nothing to print */
			return true;
		}
	}

	exc = dom_html_element_get_tag_type(node, &html_type);

	if (DOM_NO_ERR == exc) {
		if (html_type) {
//			html_context->putsp = HTML_SPACE_ADD;

			if (!html_is_preformatted() && (html_context->putsp == HTML_SPACE_NORMAL || html_context->putsp == HTML_SPACE_ADD)) {
//			unsigned char *ee = end;
//			unsigned char *nm;

//			while (!parse_element(ee, eof, &nm, NULL, NULL, &ee))
//				if (*nm == '/')
//					goto ng;
//			if (ee < eof && isspace(*ee)) {
//				put_chrs(html_context, " ", 1);
//			}
		}

#define ELEMENT_RENDER_PROLOGUE \
	ln_break(html_context, tags_dom_html_array[html_type].linebreak); \
	dom_html_element_get_id(node, &id_value); \
	if (id_value) { \
		a = memacpy(dom_string_data(id_value), dom_string_length(id_value)); \
		dom_string_unref(id_value); \
		if (a) { \
			html_context->special_f(html_context, SP_TAG, a); \
			mem_free(a); \
		} \
	}

	char *a;
	struct par_attrib old_format;
	int restore_format;
#ifdef CONFIG_CSS
	struct css_selector *selector = NULL;
#endif

	/* If the currently top element on the stack cannot contain other
	 * elements, pop it. */
	if (html_top->type == ELEMENT_WEAK) {
		pop_html_element(html_context);
	}

	/* If an element handler has temporarily disabled rendering by setting
	 * the invisible flag, skip all handling for this element.
	 *
	 * As a special case, invisible can be set to 2 or greater to indicate
	 * that processing has been temporarily disabled except when a <script>
	 * tag is encountered. This special case is necessary for 2 situations:
	 * 1. A <script> tag is contained by a block with the CSS "display"
	 * property set to "none"; or 2. one <script> tag is nested inside
	 * another, but the outer script is not processed, in which case ELinks
	 * should still run any inner script blocks.  */
	if (html_top->invisible
	    && (html_type != DOM_HTML_ELEMENT_TYPE_SCRIPT || html_top->invisible < 2)) {
		ELEMENT_RENDER_PROLOGUE
		return true;
	}

	/* Store formatting information for the currently top element on the
	 * stack before processing the new element. */
	restore_format = html_is_preformatted();
	old_format = par_format;

	/* Check for <meta refresh="..."> and <meta> cache-control directives
	 * inside <body> (see bug 700). */
#if 0
	if (html_type == DOM_HTML_ELEMENT_TYPE_META && html_context->was_body) {
		html_handle_body_meta(html_context, name - 1, eof);
		html_context->was_body = 0;
	}
#endif

#if 0
	/* If this is a style tag, parse it. */
#ifdef CONFIG_CSS
	if (html_type == DOM_HTML_ELEMENT_TYPE_STYLE && html_context->options->css_enable) {
		dom_string *media_value = NULL;
		dom_html_style_element_get_media((dom_html_style_element *)node, &media_value);

		if (media_value) {
			unsigned char *media = memacpy(dom_string_data(media_value), dom_string_byte_length(media_value));
			int support;
			dom_string_unref(media_value);
		
			support = supports_html_media_attr(media);
			mem_free_if(media);

			if (support);
#if 0
				css_parse_stylesheet(&html_context->css_styles,

					     html_context->base_href,
					     html, eof);

#endif
		}
	}
#endif
#endif

	/* If this element is inline, non-nestable, and not <li>, and the next
	 * stack item down is the same element, try to pop both elements.
	 *
	 * The effect is to close automatically any <a> or <tt> tag that
	 * encloses the current tag if it is of the same element.
	 *
	 * Otherwise, if this element is non-inline or <li> and is
	 * non-nestable, search down through the stack until 1. we find a
	 * non-killable element; 2. the element being processed is not <li> and
	 * we find a block; or 3. the element being processed is <li> and we
	 * find another <li>.  Then if the element found is the same as the
	 * current element, try to pop all elements down to and including the
	 * found element.
	 *
	 * The effect is to close automatically any <hN>, <p>, or <li> tag that
	 * encloses the current tag if it is of the same element, ignoring any
	 * intervening inline elements.
	 */
#if 1
	if (tags_dom_html_array[html_type].type == ET_NON_NESTABLE || tags_dom_html_array[html_type].type == ET_LI) {
		struct html_element *e;

		if (tags_dom_html_array[html_type].type == ET_NON_NESTABLE) {
			foreach (e, html_context->stack) {
				if (e->type < ELEMENT_KILLABLE) break;
				if (is_block_element(e) || tag_is_inline_element(html_type)) break;
			}
		} else { /* This is an <li>. */
			foreach (e, html_context->stack) {
				if (is_block_element(e) && tag_is_inline_element(html_type)) break;
				if (e->type < ELEMENT_KILLABLE) break;
				if (e->tag == html_type) break;
				//if (!c_strlcasecmp(e->name, e->namelen, name, namelen)) break;
			}
		}

//		if (!c_strlcasecmp(e->name, e->namelen, name, namelen)) {
		if (e->tag == html_type) {

			while (e->prev != (void *) &html_context->stack) {
//fprintf(stderr, "%s %d before kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
				kill_html_stack_item(html_context, e->prev);
//fprintf(stderr, "%s %d after kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
			}

			if (e->type > ELEMENT_IMMORTAL) {
//fprintf(stderr, "%s %d before kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
				kill_html_stack_item(html_context, e);
//fprintf(stderr, "%s %d after kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
			}
		}
	}
#endif
	/* Create an item on the stack for the element being processed. */
	html_stack_dup(html_context, ELEMENT_KILLABLE);
	html_top->tag = html_type;
	html_top->node = node;
	html_top->name = tags_dom_html_array[html_type].name;
	html_top->namelen = tags_dom_html_array[html_type].namelen;
//	html_top->options = attr;
	html_top->linebreak = tags_dom_html_array[html_type].linebreak;

	/* If the element has an onClick handler for scripts, make it
	 * clickable. */
#ifdef CONFIG_ECMASCRIPT

	exc = dom_string_create("onclick", sizeof("onclick") - 1, &onclick_value);

	if (DOM_NO_ERR == exc && onclick_value) {
		bool has_onclick = false;

		dom_element_has_attribute(node, onclick_value, &has_onclick);
		dom_string_unref(onclick_value);
	
		if (has_onclick) {
			/* XXX: Put something better to format.link. --pasky */
			mem_free_set(&format.link, stracpy("javascript:void(0);"));
			mem_free_set(&format.target, stracpy(html_context->base_target));
			format.style.color.foreground = format.color.clink;
			html_top->pseudo_class = ELEMENT_LINK;
			mem_free_set(&format.title, stracpy("onClick placeholder"));
			/* Er. I know. Well, double html_focusable()s shouldn't
			* really hurt. */
			//html_focusable(html_context, attr);
		}
	}
#endif

	/* Apply CSS styles. */
#ifdef CONFIG_CSS
	if ((html_top->options || html_top->node) && html_context->options->css_enable) {
		dom_string *id_value = NULL;
		dom_string *class_value = NULL;
		unsigned char *id = NULL;
		unsigned char *class_ = NULL;

		exc = dom_html_element_get_id(node, &id_value);
		if (DOM_NO_ERR == exc && id_value) {
			id = memacpy(dom_string_data(id_value), dom_string_byte_length(id_value));
			dom_string_unref(id_value);
		}

		exc = dom_html_element_get_class_name(node, &class_value);
		if (DOM_NO_ERR == exc && class_value) {
			class_ = memacpy(dom_string_data(class_value), dom_string_byte_length(class_value));
			dom_string_unref(class_value);
		}

		/* XXX: We should apply CSS otherwise as well, but that'll need
		 * some deeper changes in order to have options filled etc.
		 * Probably just applying CSS from more places, since we
		 * usually have type != ET_NESTABLE when we either (1)
		 * rescan on your own from somewhere else (2) html_stack_dup()
		 * in our own way.  --pasky */
		mem_free_set(&html_top->attr.id, id);
		mem_free_set(&html_top->attr.class_, class_);
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
#endif

	/* 1. Put any linebreaks that the element calls for, and 2. register
	 * any fragment identifier.  Step 1 cannot be done before applying CSS
	 * styles because the CSS "display" property may change the element's
	 * linebreak value.
	 *
	 * XXX: The above is wrong: ELEMENT_RENDER_PROLOGUE only looks at the
	 * linebreak value for the element_info structure, which CSS cannot
	 * modify.  -- Miciah */
	ELEMENT_RENDER_PROLOGUE

	/* Call the element's open handler. */
	tags_dom_html_array[html_type].open(renderer, node, NULL, NULL, NULL, NULL);

	/* Apply CSS styles again. */
#ifdef CONFIG_CSS
	if (selector && (html_top->options || html_top->node)) {
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

	/* If this element was not <br>, clear the was_br flag. */
	if (html_type != DOM_HTML_ELEMENT_TYPE_BR) html_context->was_br = 0;

	/* If this element is not pairable, pop its stack item now. */
	if (tags_dom_html_array[html_type].type == ET_NON_PAIRABLE) {
//fprintf(stderr, "%s %d before kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
		kill_html_stack_item(html_context, html_top);
//fprintf(stderr, "%s %d after kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
	}

	/* If we are rendering preformatted text (see above), restore the
	 * formatting attributes that were in effect before processing this
	 * element. */
	if (restore_format) par_format = old_format;

#undef ELEMENT_RENDER_PROLOGUE
		}
		return true;
	} else if (node_name == NULL) {
		DBG("Broken: root_name == NULL\n");
 		return false;
	}

	return false;
}

/**
 * Print a closing element
 *
 * \param node   The node to dump
 * \return  true on success, or false on error
 */
static bool
dump_dom_element_closing(struct source_renderer *renderer, dom_node *node)
{
	struct html_context *html_context = renderer->html_context;
	dom_exception exc;
	dom_node_type type;
	dom_html_element_type html_type;

	/* Only interested in element nodes */
	exc = dom_node_get_node_type(node, &type);

	if (exc != DOM_NO_ERR) {
		DBG("Exception raised for node_get_node_type\n");
		return false;
	} else { 
		if (type != DOM_ELEMENT_NODE) {
			/* Nothing to print */
			return true;
		}
	}
	exc = dom_html_element_get_tag_type(node, &html_type);

	if (DOM_NO_ERR == exc) {
		if (html_type) {

//fprintf(stderr, "closing: %d\n", html_type);
			struct html_element *e, *elt;
			int lnb = 0;
			int kill = 0;
			html_context->was_br = 0;
			if (tags_dom_html_array[html_type].type == ET_NON_PAIRABLE) {

//fprintf(stderr, "closing: non_pairable");
				return true;
			}
			if (tags_dom_html_array[html_type].close) {
				tags_dom_html_array[html_type].close(renderer, node, NULL, NULL, NULL, NULL);
			}
	/* dump_html_stack(html_context); */

	/* Search down through the stack until we find 1. a different element
	 * that cannot be killed or 2. the element that is currently being
	 * processed (NOT necessarily the same instance of that element).
	 *
	 * In the first case, we are done.  In the second, if this is an inline
	 * element and we found a block element while searching, we kill the
	 * found element; else (either this is inline but no block was found or
	 * this is a block), output linebreaks for all of the elements down to
	 * and including the found element and then pop all of these elements.
	 *
	 * The effects of the procedure outlined above and implemented below
	 * are 1. to allow an inline element to close any elements that it
	 * contains iff the inline element does not contain any blocks and 2.
	 * to allow blocks to close any elements that they contain.
	 *
	 * Two situations in which this behaviour may not match expectations
	 * are demonstrated by the following HTML:
	 *
	 *    <b>a<a href="file:///">b</b>c</a>d
	 *    <a href="file:///">e<b>f<div>g</a>h</div></b>
	 *
	 * ELinks will render "a" as bold text; "b" as bold, hyperlinked text;
	 * both "c" and "d" as normal (non-bold, non-hyperlinked) text (note
	 * that "</b>" closed the link because "<b>" enclosed the "<a>" open
	 * tag); "e" as hyperlinked text; and "f", "g", and "h" all as bold,
	 * hyperlinked text (note that "</a>" did not close the link because
	 * "</a>" was enclosed by "<div>", a block tag, while the "<a>" open
	 * tag was outside of the "<div>" block).
	 *
	 * Note also that close handlers are not called, which might also lead
	 * to unexpected behaviour as illustrated by the following example:
	 *
	 *    <b><q>I like fudge,</b> he said. <q>So do I,</q> she said.
	 *
	 * ELinks will render "I like fudge," with bold typeface but will only
	 * place an opening double-quotation mark before the text and no closing
	 * mark.  "he said." will be rendered normally.  "So do I," will be
	 * rendered using single-quotation marks (as for a quotation within a
	 * quotation).  "she said." will be rendered normally.  */
//fprintf(stderr, "closing: before foreach\n");

			foreach (e, html_context->stack) {
				if (is_block_element(e) && tag_is_inline_element(html_type)) kill = 1;
				if (e->tag != html_type) {
					if (e->type < ELEMENT_KILLABLE) {
						break;
					}
					else {
						continue;
					}
				}
				if (kill) {
//fprintf(stderr, "%s %d before kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
					kill_html_stack_item(html_context, e);
//fprintf(stderr, "%s %d after kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
					break;
				}
				for (elt = e; elt != (void *) &html_context->stack; elt = elt->prev) {
					if (elt->linebreak > lnb) {
						lnb = elt->linebreak;
					}
				}

		/* This hack forces a line break after a list end. It is needed
		 * when ending a list with the last <li> having no text the
		 * line_breax is 2 so the ending list's linebreak will be
		 * ignored when calling ln_break(). */
				if (html_context->was_li) {
					html_context->line_breax = 0;
				}

				ln_break(html_context, lnb);
				while (e->prev != (void *) &html_context->stack) {
//fprintf(stderr, "%s %d before kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
					kill_html_stack_item(html_context, e->prev);
//fprintf(stderr, "%s %d after kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
				}
//fprintf(stderr, "%s %d before kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
				kill_html_stack_item(html_context, e);
//fprintf(stderr, "%s %d after kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
				break;
			}
		}
		return true;
	}

	return false;
}

static bool
dump_dom_structure(struct source_renderer *renderer, dom_node *node, int depth)
{
	dom_exception exc;
	dom_node *child;

	if (depth >= HTML_MAX_DOM_STRUCTURE_DEPTH) {
		return false;
	}

	/* Print this node's entry */
	if (dump_dom_element(renderer, node, depth) == false) {
		/* There was an error; return */
		return false;
	}

	/* Get the node's first child */
	exc = dom_node_get_first_child(node, &child);
	if (exc != DOM_NO_ERR) {
		DBG("Exception raised for node_get_first_child\n");
		return false;
	} else if (child != NULL) {
		/* node has children;  decend to children's depth */
		depth++;

		/* Loop though all node's children */
		do {
			dom_node *next_child;

			/* Visit node's descendents */
			if (dump_dom_structure(renderer, child, depth) == false) {
				/* There was an error; return */
				dom_node_unref(child);
				return false;
			}

			/* Go to next sibling */
			exc = dom_node_get_next_sibling(child, &next_child);
			if (exc != DOM_NO_ERR) {
				DBG("Exception raised for "
						"node_get_next_sibling\n");
				dom_node_unref(child);
				return false;
			}

			dom_node_unref(child);
			child = next_child;
		} while (child != NULL); /* No more children */
	}

	dump_dom_element_closing(renderer, node);

	return true;
}

void
parse_html_d(unsigned char *html, unsigned char *eof,
	   struct part *part, unsigned char *head,
	   struct html_context *html_context)
{
	dom_hubbub_parser *parser = NULL;
	dom_hubbub_error error;
	dom_hubbub_parser_params params = {0};
	dom_exception exc; /* returned by libdom functions */
	dom_document *doc = NULL; /* document, loaded into libdom */
	dom_node *root = NULL; /* root element of document */

	struct source_renderer renderer;

//	unsigned char *base_pos = html;
//	int noupdate = 0;

//	params.enc = "utf-8";
//	params.fix_enc = true;
	params.enable_script = false;
	params.msg = NULL;
	params.script = NULL;
	params.ctx = NULL;
	params.daf = NULL;

	renderer.html_context = html_context;
	/* Create Hubbub parser */
	error = dom_hubbub_parser_create(&params, &parser, &doc);
	if (DOM_HUBBUB_OK != error) {
		DBG("Can't create Hubbub Parser\n");
		return;
	}

	/* Parse data */
	error = dom_hubbub_parser_parse_chunk(parser, html, eof - html);
	if (DOM_HUBBUB_OK != error) {
		dom_hubbub_parser_destroy(parser);
		DBG("Parsing errors occur %d\n", error & ~DOM_HUBBUB_HUBBUB_ERR);
		return;
	}

	/* Done parsing file */
	error = dom_hubbub_parser_completed(parser);
	if (DOM_HUBBUB_OK != error) {
		dom_hubbub_parser_destroy(parser);
		DBG("Parsing error when construct DOM\n");
		return;
	}

	/* Finished with parser */
	dom_hubbub_parser_destroy(parser);

	html_context->putsp = HTML_SPACE_SUPPRESS;
	html_context->line_breax = html_context->table_level ? 2 : 1;
	html_context->position = 0;
	html_context->was_br = 0;
	html_context->was_li = 0;
	html_context->was_body = 0;
/*	html_context->was_body_background = 0; */
	renderer.part = html_context->part = part;
	html_context->eoff = eof;
	if (head) process_head(html_context, head);

	/* Load up the input HTML file */
	if (NULL == doc) {
		DBG("Failed to load document.\n");
		return;
	}

	/* Get root element */
	exc = dom_document_get_document_element(doc, &root);
	if (DOM_NO_ERR != exc) {
		DBG("Exception raised for get_document_element\n");
		dom_node_unref(doc);
 		return;
	} else if (NULL == root) {
		DBG("Broken: root == NULL\n");
		dom_node_unref(doc);
 		return;
	}
	
	/* Dump DOM structure */
	if (dump_dom_structure(&renderer, root, 0) == false) {
		DBG("Failed to complete DOM structure dump.\n");
		dom_node_unref(root);
		dom_node_unref(doc);
		return;
	}

	dom_node_unref(root);

	/* Finished with the dom_document */
	dom_node_unref(doc);
}

#if 0
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
#endif

void
parse_html(char *html, char *eof,
	   struct part *part, char *head,
	   struct html_context *html_context)
{
	char *base_pos = html;
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
		char *name, *attr, *end;
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
			char *h = html;

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

				html = (char *) count_newline_entities(html, eof, &newlines);
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
			char *ee = end;
			char *nm;

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

/* Perform the processing of the element that should take place when its open
 * tag is encountered.  Viewing the document as a tree of elements, this
 * routine runs as a callback for pre-order traversal. */
static unsigned char *
start_element(struct element_info *ei,
              char *name, int namelen,
              char *html,
              char *eof, char *attr,
              struct html_context *html_context)
{
#define ELEMENT_RENDER_PROLOGUE \
	ln_break(html_context, ei->linebreak); \
	a = get_attr_val(attr, "id", html_context->doc_cp); \
	if (a) { \
		html_context->special_f(html_context, SP_TAG, a); \
		mem_free(a); \
	}

	char *a;
	struct par_attrib old_format;
	int restore_format;
#ifdef CONFIG_CSS
	struct css_selector *selector = NULL;
#endif

	/* If the currently top element on the stack cannot contain other
	 * elements, pop it. */
	if (html_top->type == ELEMENT_WEAK) {
		pop_html_element(html_context);
	}

	/* If an element handler has temporarily disabled rendering by setting
	 * the invisible flag, skip all handling for this element.
	 *
	 * As a special case, invisible can be set to 2 or greater to indicate
	 * that processing has been temporarily disabled except when a <script>
	 * tag is encountered. This special case is necessary for 2 situations:
	 * 1. A <script> tag is contained by a block with the CSS "display"
	 * property set to "none"; or 2. one <script> tag is nested inside
	 * another, but the outer script is not processed, in which case ELinks
	 * should still run any inner script blocks.  */
	if (html_top->invisible
	    && (ei->open != html_script || html_top->invisible < 2)) {
		ELEMENT_RENDER_PROLOGUE
		return html;
	}

	/* Store formatting information for the currently top element on the
	 * stack before processing the new element. */
	restore_format = html_is_preformatted();
	old_format = par_format;

	/* Check for <meta refresh="..."> and <meta> cache-control directives
	 * inside <body> (see bug 700). */
	if (ei->open == html_meta && html_context->was_body) {
		html_handle_body_meta(html_context, name - 1, eof);
		html_context->was_body = 0;
	}

	/* If this is a style tag, parse it. */
#ifdef CONFIG_CSS
	if (ei->open == html_style && html_context->options->css_enable) {
		char *media
			= get_attr_val(attr, "media", html_context->doc_cp);
		int support = supports_html_media_attr(media);
		mem_free_if(media);

		if (support)
			css_parse_stylesheet(&html_context->css_styles,
					     html_context->base_href,
					     html, eof);
	}
#endif

	/* If this element is inline, non-nestable, and not <li>, and the next
	 * stack item down is the same element, try to pop both elements.
	 *
	 * The effect is to close automatically any <a> or <tt> tag that
	 * encloses the current tag if it is of the same element.
	 *
	 * Otherwise, if this element is non-inline or <li> and is
	 * non-nestable, search down through the stack until 1. we find a
	 * non-killable element; 2. the element being processed is not <li> and
	 * we find a block; or 3. the element being processed is <li> and we
	 * find another <li>.  Then if the element found is the same as the
	 * current element, try to pop all elements down to and including the
	 * found element.
	 *
	 * The effect is to close automatically any <hN>, <p>, or <li> tag that
	 * encloses the current tag if it is of the same element, ignoring any
	 * intervening inline elements.
	 */
	if (ei->type == ET_NON_NESTABLE || ei->type == ET_LI) {
		struct html_element *e;

		if (ei->type == ET_NON_NESTABLE) {
			foreach (e, html_context->stack) {
				if (e->type < ELEMENT_KILLABLE) break;
				if (is_block_element(e) || is_inline_element(ei)) break;
			}
		} else { /* This is an <li>. */
			foreach (e, html_context->stack) {
				if (is_block_element(e) && is_inline_element(ei)) break;
				if (e->type < ELEMENT_KILLABLE) break;
				if (!c_strlcasecmp(e->name, e->namelen, name, namelen)) break;
			}
		}

		if (!c_strlcasecmp(e->name, e->namelen, name, namelen)) {
			while (e->prev != (void *) &html_context->stack) {
//fprintf(stderr, "%s %d before kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
				kill_html_stack_item(html_context, e->prev);
//fprintf(stderr, "%s %d after kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
			}

			if (e->type > ELEMENT_IMMORTAL) {
//fprintf(stderr, "%s %d before kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
				kill_html_stack_item(html_context, e);
//fprintf(stderr, "%s %d after kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
			}
		}
	}

	/* Create an item on the stack for the element being processed. */
	html_stack_dup(html_context, ELEMENT_KILLABLE);
	html_top->name = name;
	html_top->namelen = namelen;
	html_top->options = attr;
	html_top->linebreak = ei->linebreak;

	/* If the element has an onClick handler for scripts, make it
	 * clickable. */
#ifdef CONFIG_ECMASCRIPT
	if (has_attr(attr, "onClick", html_context->doc_cp)) {
		/* XXX: Put something better to format.link. --pasky */
		mem_free_set(&format.link, stracpy("javascript:void(0);"));
		mem_free_set(&format.target, stracpy(html_context->base_target));
		format.style.color.foreground = format.color.clink;
		html_top->pseudo_class = ELEMENT_LINK;
		mem_free_set(&format.title, stracpy("onClick placeholder"));
		/* Er. I know. Well, double html_focusable()s shouldn't
		 * really hurt. */
		html_focusable(html_context, attr);
	}
#endif

	/* Apply CSS styles. */
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
		mem_free_set(&html_top->attr.class_,
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
#endif

	/* 1. Put any linebreaks that the element calls for, and 2. register
	 * any fragment identifier.  Step 1 cannot be done before applying CSS
	 * styles because the CSS "display" property may change the element's
	 * linebreak value.
	 *
	 * XXX: The above is wrong: ELEMENT_RENDER_PROLOGUE only looks at the
	 * linebreak value for the element_info structure, which CSS cannot
	 * modify.  -- Miciah */
	ELEMENT_RENDER_PROLOGUE

	/* Call the element's open handler. */
	if (ei->open) ei->open(html_context, attr, html, eof, &html);

	/* Apply CSS styles again. */
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

	/* If this element was not <br>, clear the was_br flag. */
	if (ei->open != html_br) html_context->was_br = 0;

	/* If this element is not pairable, pop its stack item now. */
	if (ei->type == ET_NON_PAIRABLE) {
//fprintf(stderr, "%s %d before kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
		kill_html_stack_item(html_context, html_top);
//fprintf(stderr, "%s %d after kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
	}

	/* If we are rendering preformatted text (see above), restore the
	 * formatting attributes that were in effect before processing this
	 * element. */
	if (restore_format) par_format = old_format;

	return html;
#undef ELEMENT_RENDER_PROLOGUE
}

/* Perform the processing of the element that should take place when its close
 * tag is encountered.  Viewing the document as a tree of elements and assuming
 * that all tags are properly nested and closed, this routine runs as a
 * callback for post-order traversal. */
static char *
end_element(struct element_info *ei,
            char *name, int namelen,
            char *html,
            char *eof, char *attr,
            struct html_context *html_context)
{
	struct html_element *e, *elt;
	int lnb = 0;
	int kill = 0;

	html_context->was_br = 0;

	/* If this was a non-pairable tag or an <li>; perform no further
	 * processing. */
	if (ei->type == ET_NON_PAIRABLE /* || ei->type == ET_LI */)
		return html;

	/* Call the element's close handler. */
	if (ei->close) ei->close(html_context, attr, html, eof, &html);

	/* dump_html_stack(html_context); */

	/* Search down through the stack until we find 1. a different element
	 * that cannot be killed or 2. the element that is currently being
	 * processed (NOT necessarily the same instance of that element).
	 *
	 * In the first case, we are done.  In the second, if this is an inline
	 * element and we found a block element while searching, we kill the
	 * found element; else (either this is inline but no block was found or
	 * this is a block), output linebreaks for all of the elements down to
	 * and including the found element and then pop all of these elements.
	 *
	 * The effects of the procedure outlined above and implemented below
	 * are 1. to allow an inline element to close any elements that it
	 * contains iff the inline element does not contain any blocks and 2.
	 * to allow blocks to close any elements that they contain.
	 *
	 * Two situations in which this behaviour may not match expectations
	 * are demonstrated by the following HTML:
	 *
	 *    <b>a<a href="file:///">b</b>c</a>d
	 *    <a href="file:///">e<b>f<div>g</a>h</div></b>
	 *
	 * ELinks will render "a" as bold text; "b" as bold, hyperlinked text;
	 * both "c" and "d" as normal (non-bold, non-hyperlinked) text (note
	 * that "</b>" closed the link because "<b>" enclosed the "<a>" open
	 * tag); "e" as hyperlinked text; and "f", "g", and "h" all as bold,
	 * hyperlinked text (note that "</a>" did not close the link because
	 * "</a>" was enclosed by "<div>", a block tag, while the "<a>" open
	 * tag was outside of the "<div>" block).
	 *
	 * Note also that close handlers are not called, which might also lead
	 * to unexpected behaviour as illustrated by the following example:
	 *
	 *    <b><q>I like fudge,</b> he said. <q>So do I,</q> she said.
	 *
	 * ELinks will render "I like fudge," with bold typeface but will only
	 * place an opening double-quotation mark before the text and no closing
	 * mark.  "he said." will be rendered normally.  "So do I," will be
	 * rendered using single-quotation marks (as for a quotation within a
	 * quotation).  "she said." will be rendered normally.  */
	foreach (e, html_context->stack) {
		if (is_block_element(e) && is_inline_element(ei)) kill = 1;
		if (c_strlcasecmp(e->name, e->namelen, name, namelen)) {
			if (e->type < ELEMENT_KILLABLE)
				break;
			else
				continue;
		}
		if (kill) {
//fprintf(stderr, "%s %d before kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
			kill_html_stack_item(html_context, e);
//fprintf(stderr, "%s %d after kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
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
		while (e->prev != (void *) &html_context->stack) {
//fprintf(stderr, "%s %d before kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
			kill_html_stack_item(html_context, e->prev);
//fprintf(stderr, "%s %d after kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
		}
//fprintf(stderr, "%s %d before kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);
		kill_html_stack_item(html_context, e);
//fprintf(stderr, "%s %d after kill_html_stack_item html_top=%p\n", __FUNCTION__, __LINE__, html_top);

		break;
	}

	/* dump_html_stack(html_context); */

	return html;
}

static char *
process_element(char *name, int namelen, int endingtag,
                char *html, char *prev_html,
                char *eof, char *attr,
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
scan_http_equiv(char *s, char *eof, struct string *head,
		struct string *title, int cp)
{
	char *name, *attr, *he, *c;
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
		char *s1;

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

	/* FIXME (bug 784): cp is the terminal charset;
	 * should use the document charset instead.  */
	he = get_attr_val(attr, "charset", cp);
	if (he) {
		add_to_string(head, "Charset: ");
		add_to_string(head, he);
		mem_free(he);
	}

	/* FIXME (bug 784): cp is the terminal charset;
	 * should use the document charset instead.  */
	he = get_attr_val(attr, "http-equiv", cp);
	if (!he) goto se;

	add_to_string(head, he);
	mem_free(he);

	/* FIXME (bug 784): cp is the terminal charset;
	 * should use the document charset instead.  */
	c = get_attr_val(attr, "content", cp);
	if (c) {
		add_to_string(head, ": ");
		add_to_string(head, c);
	        mem_free(c);
	}

	add_crlf_to_string(head);
	goto se;
}

#ifdef CONFIG_CSS
/** Check whether ELinks claims to support any of the media types
 * listed in the media attribute of an HTML STYLE or LINK element.  */
int
supports_html_media_attr(const char *media)
{
	const char *optstr;

	/* The 1999-12-24 edition of HTML 4.01 is inconsistent on what
	 * it means if a STYLE or LINK element has no media attribute:
	 *
	 * - According to section 14.2.3 (the STYLE element),
	 *   the default value of the media attribute is "screen".
	 *   Section 12.3 (the LINK element) also refers to that
	 *   attribute definition.
	 *
	 * - Section 14.4.1 (Media-dependent cascades) however
	 *   demonstrates a LINK without a media attribute,
	 *   and explains that it then applies to all media.
	 *
	 * This was not in the HTML 4 Errata list as of 2008-01-12
	 * (error 17 was most recent).  The problem has been reported
	 * to the www-html-editor@w3.org mailing list on 2000-11-18,
	 * 2000-11-20, 2006-01-23, 2007-10-06, and 2008-01-12.
	 *
	 * In section 29.1 of the 2006-07-26 draft of XHTML 2.0, the
	 * media attribute defaults to "all".  Also, web authors often
	 * omit the attribute (perhaps they don't even consider that
	 * there are browsers with media types other than "screen"),
	 * and we'd like ELinks to use the applicable parts of those
	 * style sheets.  */
	if (media == NULL || *media == '\0')
		return 1;

	optstr = get_opt_str("document.css.media", NULL);

	while (*media != '\0') {
		const char *beg, *end;

		while (*media == ' ')
			++media;
		beg = media;
		media += strcspn(media, ",");
		if (*media == ',')
			++media;

		/* HTML 4.01 section 6.13 says each entry in the list
		 * must be truncated before the first character that
		 * is not in the allowed set.  */
		end = beg;
		while ((*end >= 0x41 && *end <= 0x5a)
		       || (*end >= 0x61 && *end <= 0x7a)
		       || (*end >= 0x30 && *end <= 0x39)
		       || *end == 0x29)
			++end;

		if (end > beg
		    && supports_css_media_type(optstr, beg, end - beg))
			return 1;
	}
	return 0;
}
#endif /* CONFIG_CSS */
