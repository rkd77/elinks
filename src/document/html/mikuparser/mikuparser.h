
#ifndef EL__DOCUMENT_HTML_MIKUPARSER_MIKUPARSER_H
#define EL__DOCUMENT_HTML_MIKUPARSER_MIKUPARSER_H

/* This is internal header for the Mikuparser engine. It should not be included
 * from anywhere outside, ideally. */

#ifdef CONFIG_DOM_HTML
#error html/mikuparser/mikuparser.h included even though DOM parser is configured to use!
#endif

#include "document/html/parser.h"
#include "util/color.h"
#include "util/lists.h"

struct css_stylesheet;
struct document_options;
struct frameset_desc;
struct html_context;
struct part;
struct uri;
enum html_special_type;


/* HTML parser stack mortality info */
enum html_element_mortality_type {
	/* Elements of this type can not be removed from the stack. This type
	 * is created by the renderer when formatting a HTML part. */
	ELEMENT_IMMORTAL,
	/* Elements of this type can only be removed by elements of the start
	 * type. This type is created whenever a HTML state is created using
	 * init_html_parser_state(). */
	/* The element has been created by*/
	ELEMENT_DONT_KILL,
	/* These elements can safely be removed from the stack by both */
	ELEMENT_KILLABLE,
	/* These elements not only cannot bear any other elements inside but
	 * any attempt to do so will cause them to terminate. This is so deadly
	 * that it affects even invisible elements. Ie. <title>foo<body>. */
	ELEMENT_WEAK,
};

struct html_element {
	LIST_HEAD(struct html_element);

	enum html_element_mortality_type type;

	struct text_attrib attr;
	struct par_attrib parattr;
	int invisible;
	unsigned char *name;
	int namelen;
	unsigned char *options;
	/* See document/html/mikuparser/parse.c's element_info.linebreak
	 * description. */
	int linebreak;
	struct frameset_desc *frameset;

	/* For the needs of CSS engine. A wannabe bitmask. */
	enum html_element_pseudo_class {
		ELEMENT_LINK = 1,
		ELEMENT_VISITED = 2,
	} pseudo_class;
};
#define is_inline_element(e) (e->linebreak == 0)
#define is_block_element(e) (e->linebreak > 0)


/* Mikuparser-specific part of html_context */

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

struct mikuparser_context {
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

	/* For parser/forms.c: */
	unsigned char *startf;
};

#define miku(html_context) ((struct mikuparser_context *) html_context->data)


/* Interface for the table handling */

int get_bgcolor(struct html_context *html_context, unsigned char *a, color_T *rgb);
void set_fragment_identifier(struct html_context *html_context,
                             unsigned char *attr_name, unsigned char *attr);
void add_fragment_identifier(struct html_context *html_context,
                             struct part *, unsigned char *attr);

/* For html/parser/forms.c,general.c,link.c,parse.c,stack.c */

/* Ensure that there are at least n successive line-breaks at the current
 * position, but don't add more than necessary to bring the current number
 * of successive line-breaks above n.
 *
 * For example, there should be two line-breaks after a <br>, but multiple
 * successive <br>'s warrant still only two line-breaks.  ln_break will be
 * called with n = 2 for each of multiple successive <br>'s, but ln_break
 * will only add two line-breaks for the entire run of <br>'s. */
void ln_break(struct html_context *html_context, int n);

int get_color(struct html_context *html_context, unsigned char *a, unsigned char *c, color_T *rgb);

/* For parser/parse.c: */

void process_head(struct html_context *html_context, unsigned char *head);
void put_chrs(struct html_context *html_context, unsigned char *start, int len);

/* For parser/link.c: */

void html_focusable(struct html_context *html_context, unsigned char *a);
void html_skip(struct html_context *html_context, unsigned char *a);
unsigned char *get_target(struct document_options *options, unsigned char *a);

#endif
