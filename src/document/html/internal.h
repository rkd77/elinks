
#ifndef EL__DOCUMENT_HTML_INTERNAL_H
#define EL__DOCUMENT_HTML_INTERNAL_H

#include "document/css/stylesheet.h"
#include "document/html/parser.h"
#include "util/lists.h"

struct document_options;
struct uri;

/* For parser/parse.c: */

void process_head(struct html_context *html_context, unsigned char *head);
void put_chrs(struct html_context *html_context, unsigned char *start, int len);

enum html_whitespace_state {
	/* Either we are starting a new "block" or the last segment of the
	 * current "block" is ending with whitespace and we should eat any
	 * leading whitespace of the next segment passed to put_chrs().
	 * This prevents HTML whitespace from indenting new blocks by one
	 * or creating two consecutive segments of whitespace in the middle
	 * of a block. */
	HTML_SPACE_SUPPRESS,

	/* Do not do anything special.  */
	HTML_SPACE_NORMAL,

	/* We should add a space when we start the next segment if it doesn't
	 * already start with whitespace. This is used in an "x  </y>  z"
	 * scenario when the parser hits </y>: it renders "x" and sets this,
	 * so that it will then render " z". XXX: Then we could of course
	 * render "x " and set -1. But we test for this value in parse_html()
	 * if we hit an opening tag of an element and potentially
	 * put_chrs(" "). That needs more investigation yet. --pasky */
	HTML_SPACE_ADD,
};

struct html_context {
#ifdef CONFIG_CSS
	/* The default stylesheet is initially merged into it. When parsing CSS
	 * from <style>-tags and external stylesheets if enabled is merged
	 * added to it. */
	struct css_stylesheet css_styles;
	struct html_element html;
#endif

	/* These are global per-document base values, alterable by the <base>
	 * element. */
	struct uri *base_href;
	unsigned char *base_target;

	struct document_options *options;

	/* doc_cp is the charset of the document, i.e. part->document->cp.
	 * It is copied here because part->document is NULL sometimes.  */
	int doc_cp;

	/* For:
	 * html/parser/parse.c
	 * html/parser/stack.c
	 * html/parser.c */
	LIST_OF(struct html_element) stack;

	/* For parser/parse.c: */
	unsigned char *eoff; /* For parser/forms.c too */
	int line_breax; /* This is for ln_break. */
	int position; /* This is the position on the document canvas relative
	               * to the current line and is maintained by put_chrs. */
	enum html_whitespace_state putsp; /* This is for the put_chrs
					   * state-machine. */
	int was_li;

	unsigned int quote_level; /* Nesting level of <q> tags. See @html_quote
				   * for why this is unsigned. */

	unsigned int was_br:1;
	unsigned int was_xmp:1;
	unsigned int was_style:1;
	unsigned int has_link_lines:1;
	unsigned int was_body:1; /* For META refresh inside <body>. */
	unsigned int was_body_background:1; /* For <HTML> with style. */
	unsigned int was_html:1; /* was <HTML> */

	/* For html/parser.c, html/renderer.c */
	int margin;

	/* For parser/forms.c: */
	unsigned char *startf;

	/* For:
	 * html/parser/parse.c
	 * html/parser.c
	 * html/renderer.c
	 * html/tables.c */
	int table_level;

	/* For:
	 * html/parser/forms.c
	 * html/parser/link.c
	 * html/parser/parse.c
	 * html/parser/stack.c
	 * html/parser.c */
	struct part *part;

	/* For:
	 * html/parser/forms.c
	 * html/parser/link.c
	 * html/parser/parse.c
	 * html/parser.c */
	/* Note that this is for usage by put_chrs only; anywhere else in
	 * the parser, one should use put_chrs. */
	void (*put_chars_f)(struct html_context *, unsigned char *, int);

	/* For:
	 * html/parser/forms.c
	 * html/parser/link.c
	 * html/parser/parse.c
	 * html/parser/stack.c
	 * html/parser.c */
	void (*line_break_f)(struct html_context *);

	/* For:
	 * html/parser/forms.c
	 * html/parser/parse.c
	 * html/parser.c */
	void *(*special_f)(struct html_context *, enum html_special_type, ...);
};

#define html_top	((struct html_element *) html_context->stack.next)
#define html_bottom	((struct html_element *) html_context->stack.prev)
#define format		(html_top->attr)
#define par_format	(html_top->parattr)

#define html_is_preformatted() (format.style.attr & AT_PREFORMATTED)

#define get_html_max_width() \
	int_max(par_format.width - (par_format.leftmargin + par_format.rightmargin), 0)

/* For parser/link.c: */

void html_focusable(struct html_context *html_context, unsigned char *a);
void html_skip(struct html_context *html_context, unsigned char *a);
unsigned char *get_target(struct document_options *options, unsigned char *a);

void
import_css_stylesheet(struct css_stylesheet *css, struct uri *base_uri,
		      const unsigned char *unterminated_url, int len);

#endif
