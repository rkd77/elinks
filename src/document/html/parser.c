/* Generic HTML parser routines */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/css/css.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/options.h"
#include "protocol/uri.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

#include "document/html/internal.h"


struct html_context *
init_html_context(struct uri *uri, struct document_options *options,
	          void (*put_chars)(struct html_context *,
	                            unsigned char *, int),
	          void (*line_break)(struct html_context *),
	          void *(*special)(struct html_context *,
	                           enum html_special_type, ...))
{
	struct html_context *html_context;
	struct html_element *e;

	html_context = mem_calloc(1, sizeof(*html_context));
	if (!html_context) return NULL;


	html_context->put_chars_f = put_chars;
	html_context->line_break_f = line_break;
	html_context->special_f = special;

	html_context->base_href = get_uri_reference(uri);
	html_context->base_target = null_or_stracpy(options->framename);

	html_context->options = options;

	html_context->table_level = 0;


	init_list(html_context->stack);
	e = mem_calloc(1, sizeof(*e));
	if (!e) {
		mem_free(html_context);
		return NULL;
	}
	add_to_list(html_context->stack, e);

	format.style.attr = 0;
	format.fontsize = 3;
	format.link = format.target = format.image = NULL;
	format.onclick = format.ondblclick = format.onmouseover = format.onhover
		= format.onfocus = format.onmouseout = format.onblur = NULL;
	format.select = NULL;
	format.form = NULL;
	format.title = NULL;

	format.style = options->default_style;
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

	par_format.bgcolor = options->default_style.bg;


#ifdef CONFIG_CSS
	html_context->css_styles.import = import_css_stylesheet;
	html_context->css_styles.import_data = html_context;

	init_css_selector_set(&html_context->css_styles.selectors);

	if (options->css_enable)
		mirror_css_stylesheet(&default_stylesheet,
				      &html_context->css_styles);
#endif

	return html_context;
}

void
done_html_context(struct html_context *html_context)
{
#ifdef CONFIG_CSS
	if (html_context->options->css_enable)
		done_css_stylesheet(&html_context->css_styles);
#endif

	mem_free(html_context->base_target);
	done_uri(html_context->base_href);

	done_html_element(html_context, html_context->stack.next);
	assertm(list_empty(html_context->stack),
		"html stack not empty after operation");
	if_assert_failed init_list(html_context->stack);

	if (html_context->data)
		mem_free(html_context->data);
	mem_free(html_context);
}


struct html_element *
dup_html_element(struct html_context *html_context)
{
	struct html_element *e;
	struct html_element *ep = html_context->stack.next;

	assertm(ep && (void *) ep != &html_context->stack, "html stack empty");
	if_assert_failed return NULL;

	e = mem_alloc(sizeof(*e));
	if (!e) return NULL;

	copy_struct(e, ep);

	if (ep->attr.link) e->attr.link = stracpy(ep->attr.link);
	if (ep->attr.target) e->attr.target = stracpy(ep->attr.target);
	if (ep->attr.image) e->attr.image = stracpy(ep->attr.image);
	if (ep->attr.title) e->attr.title = stracpy(ep->attr.title);
	if (ep->attr.select) e->attr.select = stracpy(ep->attr.select);

	e->attr.id = e->attr.class = NULL;

	/* We don't want to propagate these. */
	/* XXX: For sure? --pasky */
	e->attr.onclick = e->attr.ondblclick = e->attr.onmouseover = e->attr.onhover
		= e->attr.onfocus = e->attr.onmouseout = e->attr.onblur = NULL;

#if 0
	if (e->name) {
		if (e->attr.link) set_mem_comment(e->attr.link, e->name, e->namelen);
		if (e->attr.target) set_mem_comment(e->attr.target, e->name, e->namelen);
		if (e->attr.image) set_mem_comment(e->attr.image, e->name, e->namelen);
		if (e->attr.title) set_mem_comment(e->attr.title, e->name, e->namelen);
		if (e->attr.select) set_mem_comment(e->attr.select, e->name, e->namelen);
	}
#endif

	e->name = NULL; e->namelen = 0;

	add_to_list(html_context->stack, e);
	return e;
}

void
done_html_element(struct html_context *html_context, struct html_element *e)
{
#ifdef CONFIG_ECMASCRIPT
	unsigned char *onload = NULL;
#endif

	assert(e);
	if_assert_failed return;
	assertm((void *) e != &html_context->stack, "trying to free bad html element");
	if_assert_failed return;

#ifdef CONFIG_ECMASCRIPT
	/* As our another tiny l33t extension, we allow the onLoad attribute for
	 * any element, executing it when that element is fully loaded. */
	onload = get_attr_value(html_context, e, "onLoad");
	if (html_context->part
	    && html_context->part->document
	    && onload && *onload && *onload != '^') {
		/* XXX: The following expression alone amounts two #includes. */
		add_to_string_list(&html_context->part->document->onload_snippets,
		                   onload, -1);
	}
	if (onload) mem_free(onload);
#endif

	mem_free_if(e->attr.link);
	mem_free_if(e->attr.target);
	mem_free_if(e->attr.image);
	mem_free_if(e->attr.title);
	mem_free_if(e->attr.select);

#ifdef CONFIG_CSS
	mem_free_if(e->attr.id);
	mem_free_if(e->attr.class);
#endif

	mem_free_if(e->attr.onclick);
	mem_free_if(e->attr.ondblclick);
	mem_free_if(e->attr.onmouseover);
	mem_free_if(e->attr.onhover);
	mem_free_if(e->attr.onfocus);
	mem_free_if(e->attr.onmouseout);
	mem_free_if(e->attr.onblur);

	mem_free_if(e->data);

	del_from_list(e);
	mem_free(e);
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
