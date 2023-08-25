/* General element handlers */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasestr() */
#endif

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "document/css/apply.h"
#include "document/html/frames.h"
#include "document/html/parser/general.h"
#include "document/html/parser/link.h"
#include "document/html/parser/stack.h"
#include "document/html/parser/parse.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/html/tables.h"
#include "document/options.h"
#include "intl/charsets.h"
#include "protocol/uri.h"
#include "terminal/draw.h"
#include "util/align.h"
#include "util/box.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memdebug.h"
#include "util/memory.h"
#include "util/string.h"

/* Unsafe macros */
#include "document/html/internal.h"

void
html_section(struct html_context *html_context, char *a,
          char *xxx3, char *xxx4, char **xxx5)
{
}

void
html_span(struct html_context *html_context, char *a,
          char *xxx3, char *xxx4, char **xxx5)
{
}

void
html_bold(struct html_context *html_context, char *a,
          char *xxx3, char *xxx4, char **xxx5)
{
	elformat.style.attr |= AT_BOLD;
}

void
html_italic(struct html_context *html_context, char *a,
            char *xxx3, char *xxx4, char **xxx5)
{
	elformat.style.attr |= AT_ITALIC;
}

void
html_strike(struct html_context *html_context, char *a,
               char *xxx3, char *xxx4, char **xxx5)
{
	elformat.style.attr |= AT_STRIKE;
}

void
html_underline(struct html_context *html_context, char *a,
               char *xxx3, char *xxx4, char **xxx5)
{
	elformat.style.attr |= AT_UNDERLINE;
}


void
html_fixed(struct html_context *html_context, char *a,
           char *xxx3, char *xxx4, char **xxx5)
{
	elformat.style.attr |= AT_FIXED;
}

void
html_subscript(struct html_context *html_context, char *a,
               char *xxx3, char *xxx4, char **xxx5)
{
	put_chrs(html_context, "[", 1);
}

void
html_subscript_close(struct html_context *html_context, char *a,
                char *xxx3, char *xxx4, char **xxx5)
{
	put_chrs(html_context, "]", 1);
}

void
html_superscript(struct html_context *html_context, char *a,
                 char *xxx3, char *xxx4, char **xxx5)
{
	put_chrs(html_context, "^", 1);
}

/* TODO: Add more languages.
 * Entities can be used in these strings.  */
static char *quote_char[2] = { C_("\""), C_("'") };

void
html_quote(struct html_context *html_context, char *a,
	   char *xxx3, char *xxx4, char **xxx5)
{
	/* An HTML document containing extremely many repetitions of
	 * "<q>" could cause @html_context->quote_level to overflow.
	 * Because it is unsigned, it then wraps around to zero, and
	 * we don't get a negative array index here.  If the document
	 * then tries to close the quotes with "</q>", @html_quote_close
	 * won't let the quote level wrap back, so it will render the
	 * quotes incorrectly, but such a document probably doesn't
	 * make sense anyway.  */
	char *q = quote_char[html_context->quote_level++ % 2];

	put_chrs(html_context, q, 1);
}

void
html_quote_close(struct html_context *html_context, char *a,
		 char *xxx3, char *xxx4,
		 char **xxx5)
{
	char *q;

	if (html_context->quote_level > 0)
		html_context->quote_level--;

	q = quote_char[html_context->quote_level % 2];

	put_chrs(html_context, q, 1);
}

void
html_font(struct html_context *html_context, char *a,
          char *xxx3, char *xxx4, char **xxx5)
{
	char *al = get_attr_val(a, "size", html_context->doc_cp);

	if (al) {
		int p = 0;
		unsigned s;
		char *nn = al;
		char *end;

		if (*al == '+') p = 1, nn++;
		else if (*al == '-') p = -1, nn++;

		errno = 0;
		s = strtoul(nn, (char **) &end, 10);
		if (!errno && *nn && !*end) {
			if (s > 7) s = 7;
			if (!p) elformat.fontsize = s;
			else elformat.fontsize += p * s;
			if (elformat.fontsize < 1) elformat.fontsize = 1;
			else if (elformat.fontsize > 7) elformat.fontsize = 7;
		}
		mem_free(al);
	}
	get_color(html_context, a, "color", &elformat.style.color.foreground);
}

void
html_body(struct html_context *html_context, char *a,
          char *xxx3, char *xxx4, char **xxx5)
{
	get_color(html_context, a, "text", &elformat.style.color.foreground);
	get_color(html_context, a, "link", &elformat.color.clink);
	get_color(html_context, a, "vlink", &elformat.color.vlink);

	if (get_bgcolor(html_context, a, &elformat.style.color.background) != -1)
		html_context->was_body_background = 1;

	html_context->was_body = 1; /* this will be used by "meta inside body" */
	html_apply_canvas_bgcolor(html_context);
}

void
html_apply_canvas_bgcolor(struct html_context *html_context)
{
#ifdef CONFIG_CSS
#ifdef CONFIG_LIBCSS
	if (!html_context->options->libcss_enable)
#endif
	/* If there are any CSS twaks regarding bgcolor, make sure we will get
	 * it _and_ prefer it over bgcolor attribute. */
	do {
	if (html_context->options->css_enable)
		css_apply(html_context, html_top, &html_context->css_styles,
		          &html_context->stack);
	} while (0);
#endif

	if (par_elformat.color.background != elformat.style.color.background) {
		/* Modify the root HTML element - format_html_part() will take
		 * this from there. */
		struct html_element *e = html_bottom;

		html_context->was_body_background = 1;
		e->parattr.color.background = e->attr.style.color.background = par_elformat.color.background = elformat.style.color.background;
	}

	if (html_context->has_link_lines
	    && par_elformat.color.background != html_context->options->default_style.color.background
	    && !search_html_stack(html_context, "BODY")) {
		html_context->special_f(html_context, SP_COLOR_LINK_LINES);
	}
}

void
html_script(struct html_context *html_context, char *a,
            char *html, char *eof, char **end)
{
#if defined(CONFIG_ECMASCRIPT_SMJS) || defined(CONFIG_QUICKJS) || defined(CONFIG_MUJS)
	/* TODO: <noscript> processing. Well, same considerations apply as to
	 * CSS property display: none processing. */
	/* TODO: Charsets for external scripts. */
	char *type, *language, *src;
	int in_comment = 0;
	char *pos = NULL;
#endif

	html_skip(html_context, a);

#if defined(CONFIG_ECMASCRIPT_SMJS) || defined(CONFIG_QUICKJS) || defined(CONFIG_MUJS)
	if (html_context->was_xml_parsed) {
		return;
	}
	/* We try to process nested <script> if we didn't process the parent
	 * one. That's why's all the fuzz. */
	/* Ref:
	 * http://www.ietf.org/internet-drafts/draft-hoehrmann-script-types-03.txt
	 */
	type = get_attr_val(a, "type", html_context->doc_cp);
	if (type) {
		pos = type;

		if (!c_strncasecmp(type, "text/", 5)) {
			pos += 5;

		} else if (!c_strncasecmp(type, "application/", 12)) {
			pos += 12;

		} else {
			mem_free(type);
not_processed:
			/* Permit nested scripts and retreat. */
			html_top->invisible++;
			return;
		}

		if (!c_strncasecmp(pos, "javascript", 10)) {
			int len = strlen(pos);

			if (len > 10 && !isdigit(pos[10])) {
				mem_free(type);
				goto not_processed;
			}

		} else if (c_strcasecmp(pos, "ecmascript")
		    && c_strcasecmp(pos, "jscript")
		    && c_strcasecmp(pos, "livescript")
		    && c_strcasecmp(pos, "x-javascript")
		    && c_strcasecmp(pos, "x-ecmascript")) {
			mem_free(type);
			goto not_processed;
		}

		mem_free(type);
	}

	/* Check that the script content is ecmascript. The value of the
	 * language attribute can be JavaScript with optional version digits
	 * postfixed (like: ``JavaScript1.1'').
	 * That attribute is deprecated in favor of type by HTML 4.01 */
	language = get_attr_val(a, "language", html_context->doc_cp);
	if (language) {
		int languagelen = strlen(language);

		if (languagelen < 10
		    || (languagelen > 10 && !isdigit(language[10]))
		    || c_strncasecmp(language, "javascript", 10)) {
			mem_free(language);
			goto not_processed;
		}

		mem_free(language);
	}

	if (html_context->part->document
	    && (src = get_attr_val(a, "src", html_context->doc_cp))) {
		/* External reference. */

		char *import_url;
		struct uri *uri;

		if (!get_opt_bool("ecmascript.enable", NULL)) {
			mem_free(src);
			goto not_processed;
		}

		/* HTML <head> urls should already be fine but we can.t detect them. */
		import_url = join_urls(html_context->base_href, src);
		mem_free(src);
		if (!import_url) goto imported;

		uri = get_uri(import_url, URI_BASE);
		if (!uri) goto imported;

		/* Request the imported script as part of the document ... */
		html_context->special_f(html_context, SP_SCRIPT, uri);
		done_uri(uri);

		/* Create URL reference onload snippet. */
		insert_in_string(&import_url, 0, "^", 1);
		add_to_ecmascript_string_list(&html_context->part->document->onload_snippets,
		                   import_url, -1, html_top->name - html_context->part->document->text);

imported:
		/* Retreat. Do not permit nested scripts, tho'. */
		if (import_url) mem_free(import_url);
		return;
	}

	/* Positive, grab the rest and interpret it. */

	/* First position to the real script start. */
	while (html < eof && *html <= ' ') html++;
	if (eof - html > 4 && !strncmp(html, "<!--", 4)) {
		in_comment = 1;
		/* We either skip to the end of line or to -->. */
		for (; *html != '\n' && *html != '\r' && eof - html >= 3; html++) {
			if (!strncmp(html, "-->", 3)) {
				/* This means the document is probably broken.
				 * We will now try to process the rest of
				 * <script> contents, which is however likely
				 * to be empty. Should we try to process the
				 * comment too? Currently it seems safer but
				 * less tolerant to broken pages, if there are
				 * any like this. */
				html += 3;
				in_comment = 0;
				break;
			}
		}
	}

	*end = html;

	/* Now look ahead for the script end. The <script> contents is raw
	 * CDATA, so we just look for the ending tag and need not care for
	 * any quote marks counting etc - YET, we are more tolerant and permit
	 * </script> stuff inside of the script if the whole <script> element
	 * contents is wrapped in a comment. See i.e. Mozilla bug 26857 for fun
	 * reading regarding this. */
	for (; *end < eof; (*end)++) {
		char *name;
		int namelen;

		if (in_comment) {
			/* TODO: If we ever get some standards-quirk mode
			 * distinction, this should be disabled in the
			 * standards mode (and we should just look for CDATA
			 * end, which is "</"). --pasky */
			if (eof - *end >= 3 && !strncmp(*end, "-->", 3)) {
				/* Next iteration will jump passed the ending '>' */
				(*end) += 2;
				in_comment = 0;
			}
			continue;
			/* XXX: Scan for another comment? That's admittelly
			 * already stretching things a little bit to an
			 * extreme ;-). */
		}

		if (**end != '<')
			continue;
		/* We want to land before the closing element, that's why we
		 * don't pass @end also as the appropriate parse_element()
		 * argument. */
		if (parse_element(*end, eof, &name, &namelen, NULL, NULL))
			continue;
		if (c_strlcasecmp(name, namelen, "/script", 7))
			continue;
		/* We have won! */
		break;
	}
	if (*end >= eof) {
		/* Either the document is not completely loaded yet or it's
		 * broken. At any rate, run away screaming. */
		*end = eof; /* Just for sanity. */
		return;
	}

	if (html_context->part->document && *html != '^') {
		add_to_ecmascript_string_list(&html_context->part->document->onload_snippets,
		                   html, *end - html, html_top->name - html_context->part->document->text);
	}
#endif
}

void
html_style(struct html_context *html_context, char *a,
           char *xxx3, char *xxx4, char **xxx5)
{
	html_context->was_style = 1;
	html_skip(html_context, a);
}

void
html_style_close(struct html_context *html_context, char *a,
                 char *xxx3, char *xxx4, char **xxx5)
{
	html_context->was_style = 0;
}

void
html_html(struct html_context *html_context, char *a,
          char *xxx3, char *xxx4, char **xxx5)
{
	/* This is here just to get CSS stuff applied. */

	/* Modify the root HTML element - format_html_part() will take
	 * this from there. */
	struct html_element *e = html_bottom;

	if (par_elformat.color.background != elformat.style.color.background)
		e->parattr.color.background = e->attr.style.color.background = par_elformat.color.background = elformat.style.color.background;
}

void
html_html_close(struct html_context *html_context, char *a,
                char *xxx3, char *xxx4, char **xxx5)
{
	if (html_top->type >= ELEMENT_KILLABLE
	    && !html_context->was_body_background)
		html_apply_canvas_bgcolor(html_context);
}

void
html_head(struct html_context *html_context, char *a,
          char *xxx3, char *xxx4, char **xxx5)
{
	/* This makes sure it gets to the stack and helps tame down unclosed
	 * <title>. */
}

void
html_meta(struct html_context *html_context, char *a,
          char *xxx3, char *xxx4, char **xxx5)
{
	/* html_handle_body_meta() does all the work. */
}

/* Handles meta tags in the HTML body. */
void
html_handle_body_meta(struct html_context *html_context, char *meta,
		      char *eof)
{
	struct string head;

	if (!init_string(&head)) return;

	/* FIXME (bug 784): cp is the terminal charset;
	 * should use the document charset instead.  */
	scan_http_equiv(meta, eof, &head, NULL, html_context->options->cp);
	process_head(html_context, head.source);
	done_string(&head);
}

void
html_title(struct html_context *html_context, char *a,
           char *xxx3, char *xxx4, char **xxx5)
{
	html_top->invisible = 1;
	html_top->type = ELEMENT_WEAK;
}

void
html_center(struct html_context *html_context, char *a,
            char *xxx3, char *xxx4, char **xxx5)
{
	par_elformat.align = ALIGN_CENTER;
	if (!html_context->table_level)
		par_elformat.leftmargin = par_elformat.rightmargin = 0;
}

void
html_linebrk(struct html_context *html_context, char *a,
             char *xxx3, char *xxx4, char **xxx5)
{
	char *al = get_attr_val(a, "align", html_context->doc_cp);

	if (al) {
		if (!c_strcasecmp(al, "left")) par_elformat.align = ALIGN_LEFT;
		else if (!c_strcasecmp(al, "right")) par_elformat.align = ALIGN_RIGHT;
		else if (!c_strcasecmp(al, "center")) {
			par_elformat.align = ALIGN_CENTER;
			if (!html_context->table_level)
				par_elformat.leftmargin = par_elformat.rightmargin = 0;
		} else if (!c_strcasecmp(al, "justify")) par_elformat.align = ALIGN_JUSTIFY;
		mem_free(al);
	}
}

void
html_br(struct html_context *html_context, char *a,
        char *html, char *eof, char **end)
{
	html_linebrk(html_context, a, html, eof, end);
	if (html_context->was_br)
		ln_break(html_context, 2);
	else
		html_context->was_br = 1;
}

void
html_p(struct html_context *html_context, char *a,
       char *html, char *eof, char **end)
{
	int_lower_bound(&par_elformat.leftmargin, html_context->margin);
	int_lower_bound(&par_elformat.rightmargin, html_context->margin);
	/*par_elformat.align = ALIGN_LEFT;*/
	html_linebrk(html_context, a, html, eof, end);
}

void
html_address(struct html_context *html_context, char *a,
             char *xxx3, char *xxx4, char **xxx5)
{
	par_elformat.leftmargin++;
	par_elformat.align = ALIGN_LEFT;
}

void
html_blockquote(struct html_context *html_context, char *a,
                char *xxx3, char *xxx4, char **xxx5)
{
	par_elformat.align = ALIGN_LEFT;
	if (par_elformat.blockquote_level == 0) {
		par_elformat.orig_leftmargin = par_elformat.leftmargin;
		par_elformat.blockquote_level++;
	}
	par_elformat.blockquote_level++;
}

void
html_blockquote_close(struct html_context *html_context, char *a,
                char *xxx3, char *xxx4, char **xxx5)
{
	if (par_elformat.blockquote_level == 2) par_elformat.blockquote_level--;
	if (par_elformat.blockquote_level > 0) par_elformat.blockquote_level--;
}

void
html_h(int h, char *a,
       format_align_T default_align, struct html_context *html_context,
       char *html, char *eof, char **end)
{
	if (!par_elformat.align) par_elformat.align = default_align;
	html_linebrk(html_context, a, html, eof, end);

	h -= 2;
	if (h < 0) h = 0;

	switch (par_elformat.align) {
		case ALIGN_LEFT:
			par_elformat.leftmargin = h * 2;
			par_elformat.rightmargin = 0;
			break;
		case ALIGN_RIGHT:
			par_elformat.leftmargin = 0;
			par_elformat.rightmargin = h * 2;
			break;
		case ALIGN_CENTER:
			par_elformat.leftmargin = par_elformat.rightmargin = 0;
			break;
		case ALIGN_JUSTIFY:
			par_elformat.leftmargin = par_elformat.rightmargin = h * 2;
			break;
	}
}

void
html_h1(struct html_context *html_context, char *a,
	char *html, char *eof, char **end)
{
	elformat.style.attr |= AT_BOLD;
	html_h(1, a, ALIGN_CENTER, html_context, html, eof, end);
}

void
html_h2(struct html_context *html_context, char *a,
        char *html, char *eof, char **end)
{
	html_h(2, a, ALIGN_LEFT, html_context, html, eof, end);
}

void
html_h3(struct html_context *html_context, char *a,
        char *html, char *eof, char **end)
{
	html_h(3, a, ALIGN_LEFT, html_context, html, eof, end);
}

void
html_h4(struct html_context *html_context, char *a,
        char *html, char *eof, char **end)
{
	html_h(4, a, ALIGN_LEFT, html_context, html, eof, end);
}

void
html_h5(struct html_context *html_context, char *a,
        char *html, char *eof, char **end)
{
	html_h(5, a, ALIGN_LEFT, html_context, html, eof, end);
}

void
html_h6(struct html_context *html_context, char *a,
        char *html, char *eof, char **end)
{
	html_h(6, a, ALIGN_LEFT, html_context, html, eof, end);
}

void
html_pre(struct html_context *html_context, char *a,
         char *xxx3, char *xxx4, char **xxx5)
{
	elformat.style.attr |= AT_PREFORMATTED;
	par_elformat.leftmargin = (par_elformat.leftmargin > 1);
	par_elformat.rightmargin = 0;
}

void
html_xmp(struct html_context *html_context, char *a,
         char *html, char *eof, char **end)
{
	html_context->was_xmp = 1;
	html_pre(html_context, a, html, eof, end);
}

void
html_xmp_close(struct html_context *html_context, char *a,
               char *html, char *eof, char **end)
{
	html_context->was_xmp = 0;
}

void
html_hr(struct html_context *html_context, char *a,
        char *html, char *eof, char **end)
{
	int i/* = par_elformat.width - 10*/;
	char r = BORDER_DHLINE;
	int q = get_num(a, "size", html_context->doc_cp);

	if (q >= 0 && q < 2) r = BORDER_SHLINE;
	html_stack_dup(html_context, ELEMENT_KILLABLE);
	par_elformat.align = ALIGN_CENTER;
	mem_free_set(&elformat.link, NULL);
	elformat.form = NULL;
	html_linebrk(html_context, a, html, eof, end);
	if (par_elformat.align == ALIGN_JUSTIFY) par_elformat.align = ALIGN_CENTER;
	par_elformat.leftmargin = par_elformat.rightmargin = html_context->margin;

	i = get_width(a, "width", 1, html_context);
	if (i == -1) i = get_html_max_width();
	elformat.style.attr = AT_GRAPHICS;
	html_context->special_f(html_context, SP_NOWRAP, 1);
	while (i-- > 0) {
		put_chrs(html_context, &r, 1);
	}
	html_context->special_f(html_context, SP_NOWRAP, 0);
	ln_break(html_context, 2);
	pop_html_element(html_context);
}

void
html_table(struct html_context *html_context, char *attr,
           char *html, char *eof, char **end)
{
	if (html_context->options->tables
	    && html_context->table_level < HTML_MAX_TABLE_LEVEL) {
		format_table(attr, html, eof, end, html_context);
		ln_break(html_context, 2);

		return;
	}

	par_elformat.leftmargin = par_elformat.rightmargin = html_context->margin;
	par_elformat.align = ALIGN_LEFT;
	html_linebrk(html_context, attr, html, eof, end);
	elformat.style.attr = 0;
}

void
html_tt(struct html_context *html_context, char *a,
        char *xxx3, char *xxx4, char **xxx5)
{
}

void
html_tr(struct html_context *html_context, char *a,
        char *html, char *eof, char **end)
{
	html_linebrk(html_context, a, html, eof, end);
}

void
html_th(struct html_context *html_context, char *a,
        char *xxx3, char *xxx4, char **xxx5)
{
	/*html_linebrk(html_context, a, html, eof, end);*/
	kill_html_stack_until(html_context, 1,
	                      "TD", "TH", "", "TR", "TABLE", NULL);
	elformat.style.attr |= AT_BOLD;
	put_chrs(html_context, " ", 1);
}

void
html_td(struct html_context *html_context, char *a,
        char *xxx3, char *xxx4, char **xxx5)
{
	/*html_linebrk(html_context, a, html, eof, end);*/
	kill_html_stack_until(html_context, 1,
	                      "TD", "TH", "", "TR", "TABLE", NULL);
	elformat.style.attr &= ~AT_BOLD;
	put_chrs(html_context, " ", 1);
}

void
html_base(struct html_context *html_context, char *a,
          char *xxx3, char *xxx4, char **xxx5)
{
	char *al;

	al = get_url_val(a, "href", html_context->doc_cp);
	if (al) {
		char *base = join_urls(html_context->base_href, al);
		struct uri *uri = base ? get_uri(base, URI_NONE) : NULL;

		mem_free(al);
		mem_free_if(base);

		if (uri) {
			done_uri(html_context->base_href);
			html_context->base_href = uri;
		}
	}

	al = get_target(html_context->options, a);
	if (al) mem_free_set(&html_context->base_target, al);
}

void
html_ul(struct html_context *html_context, char *a,
        char *xxx3, char *xxx4, char **xxx5)
{
	char *al;

	/* dump_html_stack(html_context); */
	par_elformat.list_level++;
	par_elformat.list_number = 0;
	par_elformat.flags = P_DISC;

	al = get_attr_val(a, "type", html_context->doc_cp);
	if (al) {
		if (!c_strcasecmp(al, "disc"))
			par_elformat.flags = P_DISC;
		else if (!c_strcasecmp(al, "circle"))
			par_elformat.flags = P_O;
		else if (!c_strcasecmp(al, "square"))
			par_elformat.flags = P_SQUARE;
		mem_free(al);
	}
	par_elformat.leftmargin += 2 + (par_elformat.list_level > 1);
	if (!html_context->table_level)
		int_upper_bound(&par_elformat.leftmargin, par_elformat.width / 2);

	par_elformat.align = ALIGN_LEFT;
	html_top->type = ELEMENT_DONT_KILL;
}

void
html_ol(struct html_context *html_context, char *a,
        char *xxx3, char *xxx4, char **xxx5)
{
	char *al;
	int st;

	par_elformat.list_level++;
	st = get_num(a, "start", html_context->doc_cp);
	if (st == -1) st = 1;
	par_elformat.list_number = st;
	par_elformat.flags = P_NUMBER;

	al = get_attr_val(a, "type", html_context->doc_cp);
	if (al) {
		if (*al && !al[1]) {
			if (*al == '1') par_elformat.flags = P_NUMBER;
			else if (*al == 'a') par_elformat.flags = P_alpha;
			else if (*al == 'A') par_elformat.flags = P_ALPHA;
			else if (*al == 'r') par_elformat.flags = P_roman;
			else if (*al == 'R') par_elformat.flags = P_ROMAN;
			else if (*al == 'i') par_elformat.flags = P_roman;
			else if (*al == 'I') par_elformat.flags = P_ROMAN;
		}
		mem_free(al);
	}

	par_elformat.leftmargin += (par_elformat.list_level > 1);
	if (!html_context->table_level)
		int_upper_bound(&par_elformat.leftmargin, par_elformat.width / 2);

	par_elformat.align = ALIGN_LEFT;
	html_top->type = ELEMENT_DONT_KILL;
}

static struct {
	int n;
	const char *s;
} roman_tbl[] = {
	{1000,	"m"},
	{999,	"im"},
	{990,	"xm"},
	{900,	"cm"},
	{500,	"d"},
	{499,	"id"},
	{490,	"xd"},
	{400,	"cd"},
	{100,	"c"},
	{99,	"ic"},
	{90,	"xc"},
	{50,	"l"},
	{49,	"il"},
	{40,	"xl"},
	{10,	"x"},
	{9,	"ix"},
	{5,	"v"},
	{4,	"iv"},
	{1,	"i"},
	{0,	NULL}
};

static void
roman(struct string  *p, unsigned n)
{
	int i = 0;

	if (n >= 4000) {
		add_to_string(p, "---");
		return;
	}
	if (!n) {
		add_to_string(p, "o");
		return;
	}
	while (n) {
		while (roman_tbl[i].n <= n) {
			n -= roman_tbl[i].n;
			add_to_string(p, roman_tbl[i].s);
		}
		i++;
		assertm(!(n && !roman_tbl[i].n),
			"BUG in roman number converter");
		if_assert_failed break;
	}
}

void
html_li(struct html_context *html_context, char *a,
        char *xxx3, char *xxx4, char **xxx5)
{
	int t = par_elformat.flags & P_LISTMASK;

	/* When handling the code <li><li> @was_li will be 1 and it means we
	 * have to insert a line break since no list item content has done it
	 * for us. */
	if (html_context->was_li) {
		html_context->line_breax = 0;
		ln_break(html_context, 1);
	}

	/*kill_html_stack_until(html_context, 0
	                      "", "UL", "OL", NULL);*/
	if (t == P_NO_BULLET) {
		/* Print nothing. */
	} else if (!par_elformat.list_number) {
		if (t == P_O) /* Print U+25E6 WHITE BULLET. */
			put_chrs(html_context, "&#9702;", 7);
		else if (t == P_SQUARE) /* Print U+25AA BLACK SMALL SQUARE. */
			put_chrs(html_context, "&#9642;", 7);
		else /* Print U+2022 BULLET. */
			put_chrs(html_context, "&#8226;", 7);
		put_chrs(html_context, "&nbsp;", 6);
		par_elformat.leftmargin += 2;
		par_elformat.align = ALIGN_LEFT;

	} else {
		unsigned char c = 0;
		int nlen;
		int t = par_elformat.flags & P_LISTMASK;
		int s = get_num(a, "value", html_context->doc_cp);
		struct string n;

		if (!init_string(&n)) return;

		if (s != -1) par_elformat.list_number = s;

		if (t == P_ALPHA || t == P_alpha) {
			unsigned char n0;

			put_chrs(html_context, "&nbsp;", 6);
			c = 1;
			n0 = par_elformat.list_number
			       ? (par_elformat.list_number - 1) % 26
			         + (t == P_ALPHA ? 'A' : 'a')
			       : 0;
			if (n0) add_char_to_string(&n, n0);

		} else if (t == P_ROMAN || t == P_roman) {
			roman(&n, par_elformat.list_number);
			if (t == P_ROMAN) {
				char *x;

				for (x = n.source; *x; x++) *x = c_toupper(*x);
			}

		} else {
			char n0[64];
			if (par_elformat.list_number < 10) {
				put_chrs(html_context, "&nbsp;", 6);
				c = 1;
			}

			ulongcat(n0, NULL, par_elformat.list_number, (sizeof(n) - 1), 0);
			add_to_string(&n, n0);
		}

		nlen = n.length;
		put_chrs(html_context, n.source, nlen);
		put_chrs(html_context, ".&nbsp;", 7);
		par_elformat.leftmargin += nlen + c + 2;
		par_elformat.align = ALIGN_LEFT;
		done_string(&n);

		{
			struct html_element *element;

			element = search_html_stack(html_context, "ol");
			if (element)
				element->parattr.list_number = par_elformat.list_number + 1;
		}

		par_elformat.list_number = 0;
	}

	html_context->putsp = HTML_SPACE_SUPPRESS;
	html_context->line_breax = 2;
	html_context->was_li = 1;
}

void
html_dl(struct html_context *html_context, char *a,
        char *xxx3, char *xxx4, char **xxx5)
{
	par_elformat.flags &= ~P_COMPACT;
	if (has_attr(a, "compact", html_context->doc_cp))
		par_elformat.flags |= P_COMPACT;
	if (par_elformat.list_level) par_elformat.leftmargin += 5;
	par_elformat.list_level++;
	par_elformat.list_number = 0;
	par_elformat.align = ALIGN_LEFT;
	par_elformat.dd_margin = par_elformat.leftmargin;
	html_top->type = ELEMENT_DONT_KILL;
	if (!(par_elformat.flags & P_COMPACT)) {
		ln_break(html_context, 2);
		html_top->linebreak = 2;
	}
}

void
html_dt(struct html_context *html_context, char *a,
        char *xxx3, char *xxx4, char **xxx5)
{
	kill_html_stack_until(html_context, 0, "", "DL", NULL);
	par_elformat.align = ALIGN_LEFT;
	par_elformat.leftmargin = par_elformat.dd_margin;
	if (!(par_elformat.flags & P_COMPACT)
	    && !has_attr(a, "compact", html_context->doc_cp))
		ln_break(html_context, 2);
}

void
html_dd(struct html_context *html_context, char *a,
        char *xxx3, char *xxx4, char **xxx5)
{
	kill_html_stack_until(html_context, 0, "", "DL", NULL);

	par_elformat.leftmargin = par_elformat.dd_margin + 3;

	if (!html_context->table_level) {
		par_elformat.leftmargin += 5;
		int_upper_bound(&par_elformat.leftmargin, par_elformat.width / 2);
	}
	par_elformat.align = ALIGN_LEFT;
}



void
html_noframes(struct html_context *html_context, char *a,
              char *xxx3, char *xxx4, char **xxx5)
{
	struct html_element *element;

	if (!html_context->options->frames) return;

	element = search_html_stack(html_context, "frameset");
	if (element && !element->frameset) return;

	html_skip(html_context, a);
}

void
html_frame(struct html_context *html_context, char *a,
           char *xxx3, char *xxx4, char **xxx5)
{
	char *name, *src, *url;

	src = get_url_val(a, "src", html_context->doc_cp);
	if (!src) {
		url = stracpy("about:blank");
	} else {
		url = join_urls(html_context->base_href, src);
		mem_free(src);
	}
	if (!url) return;

	name = get_attr_val(a, "name", html_context->doc_cp);
	if (!name) {
		name = stracpy(url);
	} else if (!name[0]) {
		/* When name doesn't have a value */
		mem_free(name);
		name = stracpy(url);
	}
	if (!name) return;

	if (!html_context->options->frames || !html_top->frameset) {
		elformat.top_name = html_top->name;
		html_focusable(html_context, a);
		put_link_line("Frame: ", name, url, "", html_context);

	} else {
		if (html_context->special_f(html_context, SP_USED, NULL)) {
			html_context->special_f(html_context, SP_FRAME,
					       html_top->frameset, name, url);
		}
	}

	mem_free(name);
	mem_free(url);
}

void
html_frameset(struct html_context *html_context, char *a,
              char *xxx3, char *xxx4, char **xxx5)
{
	struct frameset_param fp;
	char *cols, *rows;
	int width, height;

	/* XXX: This is still not 100% correct. We should also ignore the
	 * frameset when we encountered anything 3v1l (read as: non-whitespace
	 * text/element/anything) in the document outside of <head>. Well, this
	 * is still better than nothing and it should heal up the security
	 * concerns at least because sane sites should enclose the documents in
	 * <body> elements ;-). See also bug 171. --pasky */
	if (search_html_stack(html_context, "BODY")
	    || !html_context->options->frames
	    || !html_context->special_f(html_context, SP_USED, NULL))
		return;

	cols = get_attr_val(a, "cols", html_context->doc_cp);
	if (!cols) {
		cols = stracpy("100%");
		if (!cols) return;
	}

	rows = get_attr_val(a, "rows", html_context->doc_cp);
	if (!rows) {
		rows = stracpy("100%");
	       	if (!rows) {
			mem_free(cols);
			return;
		}
	}

	if (!html_top->frameset) {
		width = html_context->options->document_width;
		height = html_context->options->box.height;
		html_context->options->needs_height = 1;
	} else {
		struct frameset_desc *frameset_desc = html_top->frameset;
		int offset;

		if (frameset_desc->box.y >= frameset_desc->box.height)
			goto free_and_return;
		offset = frameset_desc->box.x
			 + frameset_desc->box.y * frameset_desc->box.width;
		width = frameset_desc->frame_desc[offset].width;
		height = frameset_desc->frame_desc[offset].height;
	}

	fp.width = fp.height = NULL;

	parse_frame_widths(cols, width, HTML_FRAME_CHAR_WIDTH,
			   &fp.width, &fp.x);
	parse_frame_widths(rows, height, HTML_FRAME_CHAR_HEIGHT,
			   &fp.height, &fp.y);

	fp.parent = html_top->frameset;
	if (fp.x && fp.y) {
		html_top->frameset = (struct frameset_desc *)html_context->special_f(html_context, SP_FRAMESET, &fp);
	}
	mem_free_if(fp.width);
	mem_free_if(fp.height);

free_and_return:
	mem_free(cols);
	mem_free(rows);
}

void
html_noscript(struct html_context *html_context, char *a,
              char *xxx3, char *xxx4, char **xxx5)
{
	/* We shouldn't throw <noscript> away until our ECMAScript support is
	 * halfway decent. */
#if defined(CONFIG_ECMASCRIPT_SMJS) || defined(CONFIG_QUICKJS) || defined(CONFIG_MUJS)
	if (get_opt_bool("ecmascript.enable", NULL)
            && get_opt_bool("ecmascript.ignore_noscript", NULL))
		html_skip(html_context, a);
#endif
}
