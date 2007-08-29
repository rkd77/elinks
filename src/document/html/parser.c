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

	html_context = mem_calloc(1, sizeof(*html_context));
	if (!html_context) return NULL;

#ifdef CONFIG_CSS
	html_context->css_styles.import = import_css_stylesheet;
	init_css_selector_set(&html_context->css_styles.selectors);
#endif

	init_list(html_context->stack);

	html_context->put_chars_f = put_chars;
	html_context->line_break_f = line_break;
	html_context->special_f = special;

	html_context->base_href = get_uri_reference(uri);
	html_context->base_target = null_or_stracpy(options->framename);

	html_context->options = options;

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
done_html_context(struct html_context *html_context)
{
#ifdef CONFIG_CSS
	if (html_context->options->css_enable)
		done_css_stylesheet(&html_context->css_styles);
#endif

	mem_free(html_context->base_target);
	done_uri(html_context->base_href);

	if (html_context->data)
		mem_free(html_context->data);
	mem_free(html_context);
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
