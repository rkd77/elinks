/* HTML parser */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/listmenu.h"
#include "bfu/menu.h"
#include "document/css/apply.h"
#include "document/css/css.h"
#include "document/css/stylesheet.h"
#include "document/html/frames.h"
#include "document/html/parser/link.h"
#include "document/html/parser/stack.h"
#include "document/html/parser/parse.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/options.h"
#include "document/renderer.h"
#include "intl/charsets.h"
#include "protocol/date.h"
#include "protocol/header.h"
#include "protocol/uri.h"
#include "session/task.h"
#include "terminal/draw.h"
#include "util/align.h"
#include "util/box.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memdebug.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"

/* Unsafe macros */
#include "document/html/internal.h"

/* TODO: This needs rewrite. Yes, no kidding. */


int
get_color(struct html_context *html_context, unsigned char *a,
	  unsigned char *c, color_T *rgb)
{
	unsigned char *at;
	int r;

	if (!use_document_fg_colors(html_context->options))
		return -1;

	at = get_attr_val(a, c, html_context->options);
	if (!at) return -1;

	r = decode_color(at, strlen(at), rgb);
	mem_free(at);

	return r;
}

int
get_bgcolor(struct html_context *html_context, unsigned char *a, color_T *rgb)
{
	if (!use_document_bg_colors(html_context->options))
		return -1;

	return get_color(html_context, a, "bgcolor", rgb);
}

unsigned char *
get_target(struct document_options *options, unsigned char *a)
{
	unsigned char *v = get_attr_val(a, "target", options);

	if (!v) return NULL;

	if (!*v || !strcasecmp(v, "_self")) {
		mem_free_set(&v, stracpy(options->framename));
	}

	return v;
}


void
ln_break(struct html_context *html_context, int n)
{
	if (!n || html_top.invisible) return;
	while (n > html_context->line_breax) {
		html_context->line_breax++;
		html_context->line_break_f(html_context);
	}
	html_context->position = 0;
	html_context->putsp = HTML_SPACE_SUPPRESS;
}

void
put_chrs(struct html_context *html_context, unsigned char *start, int len)
{
	if (html_is_preformatted())
		html_context->putsp = HTML_SPACE_NORMAL;

	if (!len || html_top.invisible)
		return;

	switch (html_context->putsp) {
	case HTML_SPACE_NORMAL:
		break;

	case HTML_SPACE_ADD:
		html_context->put_chars_f(html_context, " ", 1);
		html_context->position++;
		html_context->putsp = HTML_SPACE_SUPPRESS;

		/* Fall thru. */

	case HTML_SPACE_SUPPRESS:
		html_context->putsp = HTML_SPACE_NORMAL;
		if (isspace(start[0])) {
			start++, len--;

			if (!len) {
				html_context->putsp = HTML_SPACE_SUPPRESS;
				return;
			}
		}

		break;
	}

	if (isspace(start[len - 1]) && !html_is_preformatted())
		html_context->putsp = HTML_SPACE_SUPPRESS;
	html_context->was_br = 0;

	html_context->put_chars_f(html_context, start, len);

	html_context->position += len;
	html_context->line_breax = 0;
	if (html_context->was_li > 0)
		html_context->was_li--;
}

void
set_fragment_identifier(struct html_context *html_context,
                        unsigned char *attr_name, unsigned char *attr)
{
	unsigned char *id_attr;

	id_attr = get_attr_val(attr_name, attr, html_context->options);

	if (id_attr) {
		html_context->special_f(html_context, SP_TAG, id_attr);
		mem_free(id_attr);
	}
}

void
add_fragment_identifier(struct html_context *html_context,
                        struct part *part, unsigned char *attr)
{
	struct part *saved_part = html_context->part;

	html_context->part = part;
	html_context->special_f(html_context, SP_TAG, attr);
	html_context->part = saved_part;
}

#ifdef CONFIG_CSS
void
import_css_stylesheet(struct css_stylesheet *css, struct uri *base_uri,
		      unsigned char *url, int len)
{
	struct html_context *html_context = css->import_data;
	unsigned char *import_url;
	struct uri *uri;

	assert(html_context);
	assert(base_uri);

	if (!html_context->options->css_enable
	    || !html_context->options->css_import)
		return;

	url = memacpy(url, len);
	if (!url) return;

	/* HTML <head> urls should already be fine but we can.t detect them. */
	import_url = join_urls(base_uri, url);
	mem_free(url);

	if (!import_url) return;

	uri = get_uri(import_url, URI_BASE);
	mem_free(import_url);

	if (!uri) return;

	/* Request the imported stylesheet as part of the document ... */
	html_context->special_f(html_context, SP_STYLESHEET, uri);

	/* ... and then attempt to import from the cache. */
	import_css(css, uri);

	done_uri(uri);
}
#endif

/* Extract the extra information that is available for elements which can
 * receive focus. Call this from each element which supports tabindex or
 * accesskey. */
/* Note that in ELinks, we support those attributes (I mean, we call this
 * function) while processing any focusable element (otherwise it'd have zero
 * tabindex, thus messing up navigation between links), thus we support these
 * attributes even near tags where we're not supposed to (like IFRAME, FRAME or
 * LINK). I think this doesn't make any harm ;). --pasky */
void
html_focusable(struct html_context *html_context, unsigned char *a)
{
	unsigned char *accesskey;
	int tabindex;

	format.accesskey = 0;
	format.tabindex = 0x80000000;

	if (!a) return;

	accesskey = get_attr_val(a, "accesskey", html_context->options);
	if (accesskey) {
		format.accesskey = accesskey_string_to_unicode(accesskey);
		mem_free(accesskey);
	}

	tabindex = get_num(a, "tabindex", html_context->options);
	if (0 < tabindex && tabindex < 32767) {
		format.tabindex = (tabindex & 0x7fff) << 16;
	}

	mem_free_if(format.onclick);
	format.onclick = get_attr_val(a, "onclick", html_context->options);

	mem_free_if(format.ondblclick);
	format.ondblclick = get_attr_val(a, "ondblclick",
	                                 html_context->options);

	mem_free_if(format.onmouseover);
	format.onmouseover = get_attr_val(a, "onmouseover",
	                                  html_context->options);
	mem_free_if(format.onhover);
	format.onhover = get_attr_val(a, "onhover", html_context->options);

	mem_free_if(format.onfocus);
	format.onfocus = get_attr_val(a, "onfocus", html_context->options);

	mem_free_if(format.onmouseout);
	format.onmouseout = get_attr_val(a, "onmouseout",
	                                 html_context->options);

	mem_free_if(format.onblur);
	format.onblur = get_attr_val(a, "onblur", html_context->options);
}

void
html_skip(struct html_context *html_context, unsigned char *a)
{
	html_top.invisible = 1;
	html_top.type = ELEMENT_DONT_KILL;
}

#ifdef CONFIG_ECMASCRIPT
int
do_html_script(struct html_context *html_context, unsigned char *a, unsigned char *html, unsigned char *eof, unsigned char **end)
{
	/* TODO: <noscript> processing. Well, same considerations apply as to
	 * CSS property display: none processing. */
	/* TODO: Charsets for external scripts. */
	unsigned char *type, *language, *src;
	int in_comment = 0;

	html_skip(html_context, a);

	/* We try to process nested <script> if we didn't process the parent
	 * one. That's why's all the fuzz. */
	/* Ref:
	 * http://www.ietf.org/internet-drafts/draft-hoehrmann-script-types-03.txt
	 */
	type = get_attr_val(a, "type", html_context->options);
	if (type) {
		unsigned char *pos = type;

		if (!strncasecmp(type, "text/", 5)) {
			pos += 5;

		} else if (!strncasecmp(type, "application/", 12)) {
			pos += 12;

		} else {
			mem_free(type);
not_processed:
			/* Permit nested scripts and retreat. */
			html_top.invisible++;
			return 1;
		}

		if (!strncasecmp(pos, "javascript", 10)) {
			int len = strlen(pos);

			if (len > 10 && !isdigit(pos[10])) {
				mem_free(type);
				goto not_processed;
			}

		} else if (strcasecmp(pos, "ecmascript")
		    && strcasecmp(pos, "jscript")
		    && strcasecmp(pos, "livescript")
		    && strcasecmp(pos, "x-javascript")
		    && strcasecmp(pos, "x-ecmascript")) {
			mem_free(type);
			goto not_processed;
		}

		mem_free(type);
	}

	/* Check that the script content is ecmascript. The value of the
	 * language attribute can be JavaScript with optional version digits
	 * postfixed (like: ``JavaScript1.1'').
	 * That attribute is deprecated in favor of type by HTML 4.01 */
	language = get_attr_val(a, "language", html_context->options);
	if (language) {
		int languagelen = strlen(language);

		if (languagelen < 10
		    || (languagelen > 10 && !isdigit(language[10]))
		    || strncasecmp(language, "javascript", 10)) {
			mem_free(language);
			goto not_processed;
		}

		mem_free(language);
	}

	if (html_context->part->document
	    && (src = get_attr_val(a, "src", html_context->options))) {
		/* External reference. */

		unsigned char *import_url;
		struct uri *uri;

		if (!get_opt_bool("ecmascript.enable")) {
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
		add_to_string_list(&html_context->part->document->onload_snippets,
		                   import_url, -1);

imported:
		/* Retreat. Do not permit nested scripts, tho'. */
		if (import_url) mem_free(import_url);
		return 1;
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
		unsigned char *name;
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
		if (strlcasecmp(name, namelen, "/script", 7))
			continue;
		/* We have won! */
		break;
	}
	if (*end >= eof) {
		/* Either the document is not completely loaded yet or it's
		 * broken. At any rate, run away screaming. */
		*end = eof; /* Just for sanity. */
		return 1;
	}

	if (html_context->part->document && *html != '^') {
		add_to_string_list(&html_context->part->document->onload_snippets,
		                   html, *end - html);
	}
	return 0;
}
#endif

void
process_head(struct html_context *html_context, unsigned char *head)
{
	unsigned char *refresh, *url;

	refresh = parse_header(head, "Refresh", NULL);
	if (!refresh) return;

	url = parse_header_param(refresh, "URL");
	if (!url) {
		/* If the URL parameter is missing assume that the
		 * document being processed should be refreshed. */
		url = get_uri_string(html_context->base_href, URI_ORIGINAL);
	}

	if (url) {
		/* Extraction of refresh time. */
		unsigned long seconds = 0;
		int valid = 1;

		/* We try to extract the refresh time, and to handle weird things
		 * in an elegant way. Among things we can have negative values,
		 * too big ones, just ';' (we assume 0 seconds in that case) and
		 * more. */
		if (*refresh != ';') {
			if (isdigit(*refresh)) {
				unsigned long max_seconds = HTTP_REFRESH_MAX_DELAY;

				errno = 0;
				seconds = strtoul(refresh, NULL, 10);
				if (errno == ERANGE || seconds > max_seconds) {
					/* Too big refresh value, limit it. */
					seconds = max_seconds;
				} else if (errno) {
					/* Bad syntax */
					valid = 0;
				}
			} else {
				/* May be a negative number, or some bad syntax. */
				valid = 0;
			}
		}

		if (valid) {
			unsigned char *joined_url = join_urls(html_context->base_href, url);

			html_focusable(html_context, NULL);

			put_link_line("Refresh: ", url, joined_url,
			              html_context->options->framename, html_context);
			html_context->special_f(html_context, SP_REFRESH, seconds, joined_url);

			mem_free(joined_url);
		}

		mem_free(url);
	}

	mem_free(refresh);

	if (!get_opt_bool("document.cache.ignore_cache_control")) {
		unsigned char *d;
		int no_cache = 0;
		time_t expires = 0;

		/* XXX: Code duplication with HTTP protocol backend. */
		/* I am not entirely sure in what order we should process these
		 * headers and if we should still process Cache-Control max-age
		 * if we already set max age to date mentioned in Expires.
		 * --jonas */
		if ((d = parse_header(head, "Pragma", NULL))) {
			if (strstr(d, "no-cache")) {
				no_cache = 1;
			}
			mem_free(d);
		}

		if (!no_cache && (d = parse_header(head, "Cache-Control", NULL))) {
			if (strstr(d, "no-cache") || strstr(d, "must-revalidate")) {
				no_cache = 1;

			} else  {
				unsigned char *pos = strstr(d, "max-age=");

				assert(!no_cache);

				if (pos) {
					/* Grab the number of seconds. */
					timeval_T max_age, seconds;

					timeval_from_seconds(&seconds, atol(pos + 8));
					timeval_now(&max_age);
					timeval_add_interval(&max_age, &seconds);

					expires = timeval_to_seconds(&max_age);
				}
			}

			mem_free(d);
		}

		if (!no_cache && (d = parse_header(head, "Expires", NULL))) {
			/* Convert date to seconds. */
			if (strstr(d, "now")) {
				timeval_T now;

				timeval_now(&now);
				expires = timeval_to_seconds(&now);
			} else {
				expires = parse_date(&d, NULL, 0, 1);
			}

			mem_free(d);
		}

		if (no_cache)
			html_context->special_f(html_context, SP_CACHE_CONTROL);
		else if (expires)
			html_context->special_f(html_context,
					       SP_CACHE_EXPIRES, expires);
	}
}




static int
look_for_map(unsigned char **pos, unsigned char *eof, struct uri *uri,
             struct document_options *options)
{
	unsigned char *al, *attr, *name;
	int namelen;

	while (*pos < eof && **pos != '<') {
		(*pos)++;
	}

	if (*pos >= eof) return 0;

	if (*pos + 2 <= eof && ((*pos)[1] == '!' || (*pos)[1] == '?')) {
		*pos = skip_comment(*pos, eof);
		return 1;
	}

	if (parse_element(*pos, eof, &name, &namelen, &attr, pos)) {
		(*pos)++;
		return 1;
	}

	if (strlcasecmp(name, namelen, "MAP", 3)) return 1;

	if (uri && uri->fragment) {
		al = get_attr_val(attr, "name", options);
		if (!al) return 1;

		if (strlcasecmp(al, -1, uri->fragment, uri->fragmentlen)) {
			mem_free(al);
			return 1;
		}

		mem_free(al);
	}

	return 0;
}

static int
look_for_tag(unsigned char **pos, unsigned char *eof,
	     unsigned char *name, int namelen, unsigned char **label)
{
	unsigned char *pos2;
	struct string str;

	if (!init_string(&str)) {
		/* Is this the right way to bail out? --jonas */
		*pos = eof;
		return 0;
	}

	pos2 = *pos;
	while (pos2 < eof && *pos2 != '<') {
		pos2++;
	}

	if (pos2 >= eof) {
		done_string(&str);
		*pos = eof;
		return 0;
	}
	if (pos2 - *pos)
		add_bytes_to_string(&str, *pos, pos2 - *pos);
	*label = str.source;

	*pos = pos2;

	if (*pos + 2 <= eof && ((*pos)[1] == '!' || (*pos)[1] == '?')) {
		*pos = skip_comment(*pos, eof);
		return 1;
	}

	if (parse_element(*pos, eof, NULL, NULL, NULL, &pos2)) return 1;

	if (strlcasecmp(name, namelen, "A", 1)
	    && strlcasecmp(name, namelen, "/A", 2)
	    && strlcasecmp(name, namelen, "MAP", 3)
	    && strlcasecmp(name, namelen, "/MAP", 4)
	    && strlcasecmp(name, namelen, "AREA", 4)
	    && strlcasecmp(name, namelen, "/AREA", 5)) {
		*pos = pos2;
		return 1;
	}

	return 0;
}

static int
look_for_link(unsigned char **pos, unsigned char *eof, struct menu_item **menu,
	      struct memory_list **ml, struct uri *href_base,
	      unsigned char *target_base, struct conv_table *ct,
	      struct document_options *options)
{
	unsigned char *attr, *href, *name, *target;
	unsigned char *label = NULL; /* shut up warning */
	struct link_def *ld;
	struct menu_item *nm;
	int nmenu;
	int namelen;

	while (*pos < eof && **pos != '<') {
		(*pos)++;
	}

	if (*pos >= eof) return 0;

	if (*pos + 2 <= eof && ((*pos)[1] == '!' || (*pos)[1] == '?')) {
		*pos = skip_comment(*pos, eof);
		return 1;
	}

	if (parse_element(*pos, eof, &name, &namelen, &attr, pos)) {
		(*pos)++;
		return 1;
	}

	if (!strlcasecmp(name, namelen, "A", 1)) {
		while (look_for_tag(pos, eof, name, namelen, &label));

		if (*pos >= eof) return 0;

	} else if (!strlcasecmp(name, namelen, "AREA", 4)) {
		unsigned char *alt = get_attr_val(attr, "alt", options);

		if (alt) {
			label = convert_string(ct, alt, strlen(alt),
			                       options->cp, CSM_DEFAULT,
			                       NULL, NULL, NULL);
			mem_free(alt);
		} else {
			label = NULL;
		}

	} else if (!strlcasecmp(name, namelen, "/MAP", 4)) {
		/* This is the only successful return from here! */
		add_to_ml(ml, *menu, NULL);
		return 0;

	} else {
		return 1;
	}

	target = get_target(options, attr);
	if (!target) target = null_or_stracpy(target_base);
	if (!target) target = stracpy("");
	if (!target) {
		mem_free_if(label);
		return 1;
	}

	ld = mem_alloc(sizeof(*ld));
	if (!ld) {
		mem_free_if(label);
		mem_free(target);
		return 1;
	}

	href = get_url_val(attr, "href", options);
	if (!href) {
		mem_free_if(label);
		mem_free(target);
		mem_free(ld);
		return 1;
	}


	ld->link = join_urls(href_base, href);
	mem_free(href);
	if (!ld->link) {
		mem_free_if(label);
		mem_free(target);
		mem_free(ld);
		return 1;
	}


	ld->target = target;
	for (nmenu = 0; !mi_is_end_of_menu(&(*menu)[nmenu]); nmenu++) {
		struct link_def *ll = (*menu)[nmenu].data;

		if (!strcmp(ll->link, ld->link) &&
		    !strcmp(ll->target, ld->target)) {
			mem_free(ld->link);
			mem_free(ld->target);
			mem_free(ld);
			mem_free_if(label);
			return 1;
		}
	}

	if (label) {
		clr_spaces(label);

		if (!*label) {
			mem_free(label);
			label = NULL;
		}
	}

	if (!label) {
		label = stracpy(ld->link);
		if (!label) {
			mem_free(target);
			mem_free(ld->link);
			mem_free(ld);
			return 1;
		}
	}

	nm = mem_realloc(*menu, (nmenu + 2) * sizeof(*nm));
	if (nm) {
		*menu = nm;
		memset(&nm[nmenu], 0, 2 * sizeof(*nm));
		nm[nmenu].text = label;
		nm[nmenu].func = map_selected;
		nm[nmenu].data = ld;
		nm[nmenu].flags = NO_INTL;
	}

	add_to_ml(ml, ld, ld->link, ld->target, label, NULL);

	return 1;
}


int
get_image_map(unsigned char *head, unsigned char *pos, unsigned char *eof,
	      struct menu_item **menu, struct memory_list **ml, struct uri *uri,
	      struct document_options *options, unsigned char *target_base,
	      int to, int def, int hdef)
{
	struct conv_table *ct;
	struct string hd;

	if (!init_string(&hd)) return -1;

	if (head) add_to_string(&hd, head);
	scan_http_equiv(pos, eof, &hd, NULL, options);
	ct = get_convert_table(hd.source, to, def, NULL, NULL, hdef);
	done_string(&hd);

	*menu = mem_calloc(1, sizeof(**menu));
	if (!*menu) return -1;

	while (look_for_map(&pos, eof, uri, options));

	if (pos >= eof) {
		mem_free(*menu);
		return -1;
	}

	*ml = NULL;

	while (look_for_link(&pos, eof, menu, ml, uri, target_base, ct, options))
		;

	if (pos >= eof) {
		freeml(*ml);
		mem_free(*menu);
		return -1;
	}

	return 0;
}




struct html_element *
init_html_parser_state(struct html_context *html_context,
                       enum html_element_type type,
                       int align, int margin, int width)
{
	struct html_element *element;

	html_stack_dup(html_context, type);
	element = &html_top;

	par_format.align = align;

	if (type <= ELEMENT_IMMORTAL) {
		par_format.leftmargin = margin;
		par_format.rightmargin = margin;
		par_format.width = width;
		par_format.list_level = 0;
		par_format.list_number = 0;
		par_format.dd_margin = 0;
		html_top.namelen = 0;
	}

	return element;
}



void
done_html_parser_state(struct html_context *html_context,
                       struct html_element *element)
{
	html_context->line_breax = 1;

	while (&html_top != element) {
		kill_html_stack_item(html_context, &html_top);
#if 0
		/* I've preserved this bit to show an example of the Old Code
		 * of the Mikulas days (I _HOPE_ it's by Mikulas, at least ;-).
		 * I think this assert() can never fail, for one. --pasky */
		assertm(&html_top && (void *) &html_top != (void *) &html_stack,
			"html stack trashed");
		if_assert_failed break;
#endif
	}

	html_top.type = ELEMENT_KILLABLE;
	kill_html_stack_item(html_context, &html_top);

}

struct html_context *
init_html_parser(struct uri *uri, struct document_options *options,
		 unsigned char *start, unsigned char *end,
		 struct string *head, struct string *title,
		 void (*put_chars)(struct html_context *, unsigned char *, int),
		 void (*line_break)(struct html_context *),
		 void *(*special)(struct html_context *, enum html_special_type, ...))
{
	struct html_context *html_context;
	struct html_element *e;

	assert(uri && options);
	if_assert_failed return NULL;

	html_context = mem_calloc(1, sizeof(*html_context));
	if (!html_context) return NULL;

#ifdef CONFIG_CSS
	html_context->css_styles.import = import_css_stylesheet;
	init_list(html_context->css_styles.selectors);
#endif

	init_list(html_context->stack);

	html_context->startf = start;
	html_context->put_chars_f = put_chars;
	html_context->line_break_f = line_break;
	html_context->special_f = special;

	html_context->base_href = get_uri_reference(uri);
	html_context->base_target = null_or_stracpy(options->framename);

	html_context->options = options;

	scan_http_equiv(start, end, head, title, options);

	e = mem_calloc(1, sizeof(*e));
	if (!e) return NULL;
	add_to_list(html_context->stack, e);

	format.style.attr = 0;
	format.fontsize = 3;
	format.link = format.target = format.image = NULL;
	format.onclick = format.ondblclick = format.onmouseover = format.onhover
		= format.onfocus = format.onmouseout = format.onblur = NULL;
	format.select = NULL;
	format.form = NULL;
	format.title = NULL;

	format.style.fg = options->default_fg;
	format.style.bg = options->default_bg;
	format.clink = options->default_link;
	format.vlink = options->default_vlink;
#ifdef CONFIG_BOOKMARKS
	format.bookmark_link = options->default_bookmark_link;
#endif
	format.image_link = options->default_image_link;

	par_format.align = ALIGN_LEFT;
	par_format.leftmargin = options->margin;
	par_format.rightmargin = options->margin;

	par_format.width = options->box.width;
	par_format.list_level = par_format.list_number = 0;
	par_format.dd_margin = options->margin;
	par_format.flags = P_NONE;

	par_format.bgcolor = options->default_bg;

	html_top.invisible = 0;
	html_top.name = NULL;
   	html_top.namelen = 0;
	html_top.options = NULL;
	html_top.linebreak = 1;
	html_top.type = ELEMENT_DONT_KILL;

	html_context->has_link_lines = 0;
	html_context->table_level = 0;

#ifdef CONFIG_CSS
	html_context->css_styles.import_data = html_context;

	if (options->css_enable)
		mirror_css_stylesheet(&default_stylesheet,
				      &html_context->css_styles);
#endif

	return html_context;
}

void
done_html_parser(struct html_context *html_context)
{
#ifdef CONFIG_CSS
	if (html_context->options->css_enable)
		done_css_stylesheet(&html_context->css_styles);
#endif

	mem_free(html_context->base_target);
	done_uri(html_context->base_href);

	kill_html_stack_item(html_context, html_context->stack.next);

	assertm(list_empty(html_context->stack),
		"html stack not empty after operation");
	if_assert_failed init_list(html_context->stack);

	mem_free(html_context);
}
