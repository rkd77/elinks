#ifndef EL__DOCUMENT_HTML_PARSER_H
#define EL__DOCUMENT_HTML_PARSER_H

/* This is generic interface for HTML parsers, as used by mainly the HTML
 * renderer. Both Mikuparser and DOM parser must conform to this. These
 * prototypes are only here, not repeated in parser-specific headers. */

#include "document/css/stylesheet.h"
#include "document/format.h"
#include "intl/charsets.h" /* unicode_val_T */
#include "util/align.h"
#include "util/color.h"

struct css_stylesheet;
struct document_options;
struct form_control;
struct memory_list;
struct menu_item;
struct part;
struct string;
struct uri;
enum html_special_type; /* Gateway to the renderer. Defined in renderer.h. */


/* These structures describe various formatting and other attributes of elements
 * and boxes. */

struct text_attrib {
	struct text_style style;

	int fontsize;
	unsigned char *link;
	unsigned char *target;
	unsigned char *image;
	unsigned char *title;
	struct form_control *form;
	color_T clink;
	color_T vlink;
#ifdef CONFIG_BOOKMARKS
	color_T bookmark_link;
#endif
	color_T image_link;

#ifdef CONFIG_CSS
	/* Bug 766: CSS speedup.  56% of CPU time was going to
	 * get_attr_value().  Of those calls, 97% were asking for "id"
	 * or "class".  So cache the results.  start_element() sets up
	 * these pointers if html_context->options->css_enable;
	 * otherwise they remain NULL. */
	unsigned char *id;
	unsigned char *class;
#endif

	unsigned char *select;
	int select_disabled;
	unsigned int tabindex;
	unicode_val_T accesskey;

	unsigned char *onclick;
	unsigned char *ondblclick;
	unsigned char *onmouseover;
	unsigned char *onhover;
	unsigned char *onfocus;
	unsigned char *onmouseout;
	unsigned char *onblur;
};

/* This enum is pretty ugly, yes ;). */
enum format_list_flag {
	P_NONE = 0,

	P_NUMBER = 1,
	P_alpha = 2,
	P_ALPHA = 3,
	P_roman = 4,
	P_ROMAN = 5,

	P_STAR = 1,
	P_O = 2,
	P_PLUS = 3,

	P_LISTMASK = 7,

	P_COMPACT = 8,
};

struct par_attrib {
	enum format_align align;
	int leftmargin;
	int rightmargin;
	int width;
	int list_level;
	unsigned list_number;
	int dd_margin;
	enum format_list_flag flags;
	color_T bgcolor;
};

/* The HTML parser context. */

struct html_context {
#ifdef CONFIG_CSS
	/* The default stylesheet is initially merged into it. When parsing CSS
	 * from <style>-tags and external stylesheets if enabled is merged
	 * added to it. */
	struct css_stylesheet css_styles;
#endif

	/* These are global per-document base values, alterable by the <base>
	 * element. */
	struct uri *base_href;
	unsigned char *base_target;

	struct document_options *options;

	/* doc_cp is the charset of the document, i.e. part->document->cp.
	 * It is copied here because part->document is NULL sometimes.  */
	int doc_cp;

	/* For html/parser.c, html/renderer.c */
	int margin;

	/* For:
	 * html/parser/parse.c
	 * html/parser.c
	 * html/renderer.c
	 * html/tables.c */
	int table_level;

	struct part *part;

	/* Note that for Mikuparser, this is for usage by put_chrs only;
	 * anywhere else in the parser, one should use put_chrs. */
	void (*put_chars_f)(struct html_context *, unsigned char *, int);

	void (*line_break_f)(struct html_context *);

	void *(*special_f)(struct html_context *, enum html_special_type, ...);

	/* Engine-specific data */
	void *data;
};


/* Major lifetime of the parser for the whole document */
struct html_context *init_html_parser(struct uri *uri,
		       struct document_options *options,
		       unsigned char *start, unsigned char *end,
		       struct string *head, struct string *title,
		       void (*put_chars)(struct html_context *,
			                 unsigned char *, int),
		       void (*line_break)(struct html_context *),
		       void *(*special)(struct html_context *,
			                enum html_special_type, ...));
void done_html_parser(struct html_context *html_context);


/* Set up parser "sub-state"; the parser may be invoked on various
 * segments of the document repeatedly (in practice it is used for
 * re-parsing table contents); is_root is set for the "main" invocation. */
void *init_html_parser_state(struct html_context *html_context,
                             int is_root, int align, int margin, int width);
void done_html_parser_state(struct html_context *html_context,
		            void *state);


/* Do the parsing job, calling callbacks specified at init_html_parser()
 * time as required. */
void parse_html(unsigned char *html, unsigned char *eof, struct part *part,
		unsigned char *head, struct html_context *html_context);


/* This examines imgmap in the given document and produces a BFU-ish menu
 * for it. */
int get_image_map(unsigned char *head, unsigned char *pos, unsigned char *eof,
                  struct menu_item **menu, struct memory_list **ml,
		  struct uri *uri, struct document_options *options,
		  unsigned char *target_base, int to, int def, int hdef);



/*********************************************************************/
/* Follows interface for functions common for all parser engines, mostly
 * implemented in parser.c. */

/* Generic html_context lifetime; called from *_html_parser(). */
struct html_context *init_html_context(struct uri *uri, struct document_options *options,
                                       void (*put_chars)(struct html_context *,
                                                         unsigned char *, int),
                                       void (*line_break)(struct html_context *),
                                       void *(*special)(struct html_context *,
				                        enum html_special_type, ...));
void done_html_context(struct html_context *html_context);

#ifdef CONFIG_CSS
void import_css_stylesheet(struct css_stylesheet *css, struct uri *base_uri,
		           unsigned char *url, int len);
#endif

#endif
