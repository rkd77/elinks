/* HTML DOM: Element-specific functions */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "document/css/parser.h"
#include "document/html/domparser/domparser.h"
#include "document/html/domparser/elements.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/options.h"
#include "dom/sgml/html/html.h"
#include "dom/node.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

#include "document/html/internal.h"


/* Sorted alphabetically */


static void
a_begins(struct html_context *html_context)
{
	unsigned char *href;

	href = get_dom_element_attr_uri(domelem(html_top)->node, HTML_ATTRIBUTE_HREF, html_context);
	if (href)
		mem_free_set(&format.link, href);
}

static void
base_pop(struct html_context *html_context)
{
	struct dom_node *node = domelem(html_top)->node;
	unsigned char *href;
	struct uri *uri;

	href = get_dom_element_attr_uri(node, HTML_ATTRIBUTE_HREF, html_context);
	if (!href) return;

	uri = get_uri(href, 0);
	mem_free(href);
	if (!uri) return;

	done_uri(html_context->base_href);
	html_context->base_href = uri;
}

static void
html_body_begins(struct html_context *html_context)
{
	/* Totally ugly hack. We just need box model. */
	html_context->special_f(html_context, SP_DEFAULT_BACKGROUND, format.style.bg);
}

static void
link_pop(struct html_context *html_context)
{
#ifdef CONFIG_CSS
	struct dom_node *node = domelem(html_top)->node;
	struct dom_string *ds;

	ds = get_dom_element_attr(node, HTML_ATTRIBUTE_REL);
	if (!ds || strlcmp(ds->string, ds->length, "stylesheet", 10))
		return;

	ds = get_dom_element_attr(node, HTML_ATTRIBUTE_HREF);
	if (!ds)
		return;

	import_css_stylesheet(&html_context->css_styles,
			      html_context->base_href, ds->string, ds->length);
#endif
}

static void
style_pop(struct html_context *html_context)
{
#ifdef CONFIG_CSS
	struct dom_node *node = domelem(html_top)->node;
	struct dom_node_list **list;
	struct dom_node *child;
	int index;

	if (!html_context->options->css_enable)
		return;

	/* We are interested in anything but attributes ... */
	list = get_dom_node_list_by_type(node, DOM_NODE_CDATA_SECTION);
	if (!list || !*list)
		return;

	foreach_dom_node (*list, child, index) {
		struct dom_string *value;

		switch (child->type) {
		case DOM_NODE_CDATA_SECTION:
		case DOM_NODE_COMMENT:
		case DOM_NODE_TEXT:
			value = get_dom_node_value(child);
			if (!value)
				continue;
			css_parse_stylesheet(&html_context->css_styles,
					html_context->base_href,
					value->string,
					value->string + value->length);
		}
	}
#endif
}

static void
title_pop(struct html_context *html_context)
{
	struct dom_node *node = domelem(html_top)->node;
	struct dom_node *title;

	title = get_dom_node_child(node, DOM_NODE_TEXT, 0);

	if (title && !domctx(html_context)->title->length)
		add_bytes_to_string(domctx(html_context)->title,
				    title->string.string,
				    title->string.length);
}


static struct {
	int type;
	struct dom_element_handlers handlers;
} ehr[] = {
	{ HTML_ELEMENT_A,	{ a_begins, NULL } },
	{ HTML_ELEMENT_BASE,	{ NULL, base_pop } },
	{ HTML_ELEMENT_BODY,	{ html_body_begins, NULL } },
	{ HTML_ELEMENT_HTML,	{ html_body_begins, NULL } },
	{ HTML_ELEMENT_LINK,	{ NULL, link_pop } },
	{ HTML_ELEMENT_STYLE,	{ NULL, style_pop } },
	{ HTML_ELEMENT_TITLE,	{ NULL, title_pop } },
};

struct dom_element_handlers dom_element_handlers[HTML_ELEMENTS];

void
dom_setup_element_handlers(void)
{
	static int setup_done;
	int i;

	if (setup_done)
		return;
	else
		setup_done++;

	for (i = 0; i < sizeof(ehr) / sizeof(*ehr); i++) {
		dom_element_handlers[ehr[i].type] = ehr[i].handlers;
	}
}
