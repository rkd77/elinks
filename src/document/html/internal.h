
#ifndef EL__DOCUMENT_HTML_INTERNAL_H
#define EL__DOCUMENT_HTML_INTERNAL_H

#include "document/css/stylesheet.h"
#include "util/lists.h"

struct document_options;
struct sgml_parser;
struct uri;
enum html_special_type;


/* The HTML parser context. It is also heavily used by the renderer so DOM
 * parser must use it as well. */

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

#ifdef CONFIG_DOM_HTML

#include "document/html/domparser/domparser.h"

#define format		(domctx(html_context)->attr)
#define par_format	(domctx(html_context)->parattr)

#else

#include "document/html/mikuparser/mikuparser.h"

#define html_top	((struct html_element *) miku(html_context)->stack.next)
#define html_bottom	((struct html_element *) miku(html_context)->stack.prev)
#define format		(html_top->attr)
#define par_format	(html_top->parattr)

#define get_html_max_width() \
	int_max(par_format.width - (par_format.leftmargin + par_format.rightmargin), 0)

#endif

#define html_is_preformatted() (format.style.attr & AT_PREFORMATTED)


#endif
