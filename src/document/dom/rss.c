/* DOM-based RSS renderer */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/css/css.h"
#include "document/css/parser.h"
#include "document/css/stylesheet.h"
#include "document/document.h"
#include "document/dom/util.h"
#include "document/dom/rss.h"
#include "dom/sgml/rss/rss.h"
#include "dom/node.h"
#include "dom/stack.h"
#include "intl/charsets.h"
#include "util/error.h"
#include "util/memory.h"


enum rss_style {
	RSS_STYLE_TITLE,
	RSS_STYLE_AUTHOR,
	RSS_STYLE_AUTHOR_DATE_SEP,
	RSS_STYLE_DATE,
	RSS_STYLES,
};

struct rss_renderer {
	struct screen_char styles[RSS_STYLES];

	struct dom_node *channel;
	struct dom_node_list *items;
	struct dom_node *item;
	struct dom_node *node;
	struct dom_string text;
};


static enum dom_code
dom_rss_push_element(struct dom_stack *stack, struct dom_node *node, void *xxx)
{
	struct dom_renderer *renderer = stack->current->data;
	struct rss_renderer *rss = renderer->data;

	assert(node && node->parent && renderer && renderer->document);

	switch (node->data.element.type) {
	case RSS_ELEMENT_CHANNEL:
		/* The stack should have: #document * channel */
		if (stack->depth == 3 && !rss->channel)
			rss->channel = node;
		break;

	case RSS_ELEMENT_ITEM:
		/* The stack should have: #document * channel item */
#if 0
		/* Don't be so strict ... */
		if (stack->depth != 4)
			break;
#endif
		/* ... but be exclusive. */
		if (!rss->item) {
			add_to_dom_node_list(&rss->items, node, -1);
			rss->item = node;
		}
		break;

	case RSS_ELEMENT_LINK:
	case RSS_ELEMENT_DESCRIPTION:
	case RSS_ELEMENT_TITLE:
	case RSS_ELEMENT_AUTHOR:
	case RSS_ELEMENT_PUBDATE:
		if (rss->node == node->parent)
			rss->node = node;
	}

	return DOM_CODE_OK;
}

static enum dom_code
dom_rss_pop_element(struct dom_stack *stack, struct dom_node *node, void *xxx)
{
	struct dom_renderer *renderer = stack->current->data;
	struct rss_renderer *rss = renderer->data;
	struct dom_node_list **list;

	assert(node && node->parent && renderer && renderer->document);

	switch (node->data.element.type) {
	case RSS_ELEMENT_ITEM:
		if (is_dom_string_set(&rss->text))
			done_dom_string(&rss->text);
		rss->item = NULL;
		break;

	case RSS_ELEMENT_LINK:
	case RSS_ELEMENT_DESCRIPTION:
	case RSS_ELEMENT_TITLE:
	case RSS_ELEMENT_AUTHOR:
	case RSS_ELEMENT_PUBDATE:
		if (!is_dom_string_set(&rss->text)
		    || rss->item != node->parent
		    || rss->node != node)
			break;

		/* Replace any child nodes with the normalized text node.
		 * We are getting rid of "inner HTML". */
		list = get_dom_node_list(node->parent, node);
		done_dom_node_list(*list);
		if (!add_dom_node(node, DOM_NODE_TEXT, &rss->text))
			done_dom_string(&rss->text);
		rss->node = NULL;
		break;
	}

	return DOM_CODE_OK;
}


static struct dom_string *
get_rss_text(struct dom_node *node, enum rss_element_type type)
{
	node = get_dom_node_child(node, DOM_NODE_ELEMENT, type);

	if (!node) return NULL;

	node = get_dom_node_child(node, DOM_NODE_TEXT, 0);

	return node ? &node->string: NULL;
}

static void
render_rss_item(struct dom_renderer *renderer, struct dom_node *item)
{
	struct rss_renderer *rss = renderer->data;
	struct dom_string *title  = get_rss_text(item, RSS_ELEMENT_TITLE);
	struct dom_string *link   = get_rss_text(item, RSS_ELEMENT_LINK);
	struct dom_string *author = get_rss_text(item, RSS_ELEMENT_AUTHOR);
	struct dom_string *date   = get_rss_text(item, RSS_ELEMENT_PUBDATE);

	if (title && is_dom_string_set(title)) {
		if (item == rss->channel) {
			unsigned char *str;

			str = convert_string(renderer->convert_table,
					     title->string, title->length,
					     renderer->document->options.cp,
					     CSM_DEFAULT, NULL, NULL, NULL);
			if (str)
				renderer->document->title = str;
		}
		render_dom_text(renderer, &rss->styles[RSS_STYLE_TITLE],
				title->string, title->length);
	}

	if (link && is_dom_string_set(link)) {
		X(renderer)++;
		add_dom_link(renderer, "[link]", 6, link->string, link->length);
	}

	/* New line, and indent */
	Y(renderer)++;
	X(renderer) = 0;

	if (author && is_dom_string_set(author)) {
		render_dom_text(renderer, &rss->styles[RSS_STYLE_AUTHOR],
				author->string, author->length);
	}

	if (date && is_dom_string_set(date)) {
		if (author && is_dom_string_set(author)) {
			render_dom_text(renderer, &rss->styles[RSS_STYLE_AUTHOR_DATE_SEP],
					" - ", 3);
		}

		render_dom_text(renderer, &rss->styles[RSS_STYLE_DATE],
				date->string, date->length);
	}

	if ((author && is_dom_string_set(author))
	    || (date && is_dom_string_set(date))) {
		/* New line, and indent */
		Y(renderer)++;
		X(renderer) = 0;
	}
}


static enum dom_code
dom_rss_push_document(struct dom_stack *stack, struct dom_node *root, void *xxx)
{
	struct dom_renderer *renderer = stack->current->data;
	struct document *document = renderer->document;
	struct rss_renderer *rss;
	enum rss_style type;

	struct css_stylesheet *css = &default_stylesheet;
	{
		static int i_want_struct_module_for_dom;

		if (!i_want_struct_module_for_dom) {
			static const unsigned char default_colors[] =
				"title		{ color: lightgreen } "
				"author		{ color: aqua }"
				"author-date-sep{ color: aqua }"
				"date		{ color: aqua }";
			unsigned char *styles = (unsigned char *) default_colors;

			i_want_struct_module_for_dom = 1;
			/* When someone will get here earlier than at 4am,
			 * this will be done in some init function, perhaps
			 * not overriding the user's default stylesheet. */
			css_parse_stylesheet(css, NULL, styles, styles + sizeof(default_colors));
		}
	}

	rss = renderer->data = mem_calloc(1, sizeof(*rss));

	/* Initialize styles. */

	for (type = 0; type < RSS_STYLES; type++) {
		struct screen_char *template = &rss->styles[type];
		static const unsigned char *names[RSS_STYLES] =
			{ "title", "author", "author-date-sep", "date" };
		struct css_selector *selector = NULL;

		selector = find_css_selector(&css->selectors,
					     CST_ELEMENT, CSR_ROOT,
					     names[type], strlen(names[type]));
		init_template_by_style(template, &document->options,
				       selector ? &selector->properties : NULL);
	}

	return DOM_CODE_OK;
}

static enum dom_code
dom_rss_pop_document(struct dom_stack *stack, struct dom_node *root, void *xxx)
{
	struct dom_renderer *renderer = stack->current->data;
	struct rss_renderer *rss = renderer->data;

	if (!rss->channel)
		return DOM_CODE_OK;

	render_rss_item(renderer, rss->channel);

	if (rss->items) {
		struct dom_node *node;
		int index;

		foreach_dom_node (rss->items, node, index) {
			Y(renderer)++;
			X(renderer) = 0;
			render_rss_item(renderer, node);
		}
	}

	if (is_dom_string_set(&rss->text))
		done_dom_string(&rss->text);
	mem_free_if(rss->items);

	done_dom_node(root);

	mem_free(rss);

	return DOM_CODE_OK;
}


struct dom_stack_context_info dom_rss_renderer_context_info = {
	/* Object size: */			0,
	/* Push: */
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ dom_rss_push_element,
		/* DOM_NODE_ATTRIBUTE		*/ NULL,
		/* DOM_NODE_TEXT		*/ NULL,
		/* DOM_NODE_CDATA_SECTION	*/ NULL,
		/* DOM_NODE_ENTITY_REFERENCE	*/ NULL,
		/* DOM_NODE_ENTITY		*/ NULL,
		/* DOM_NODE_PROC_INSTRUCTION	*/ NULL,
		/* DOM_NODE_COMMENT		*/ NULL,
		/* DOM_NODE_DOCUMENT		*/ dom_rss_push_document,
		/* DOM_NODE_DOCUMENT_TYPE	*/ NULL,
		/* DOM_NODE_DOCUMENT_FRAGMENT	*/ NULL,
		/* DOM_NODE_NOTATION		*/ NULL,
	},
	/* Pop: */
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ dom_rss_pop_element,
		/* DOM_NODE_ATTRIBUTE		*/ NULL,
		/* DOM_NODE_TEXT		*/ NULL,
		/* DOM_NODE_CDATA_SECTION	*/ NULL,
		/* DOM_NODE_ENTITY_REFERENCE	*/ NULL,
		/* DOM_NODE_ENTITY		*/ NULL,
		/* DOM_NODE_PROC_INSTRUCTION	*/ NULL,
		/* DOM_NODE_COMMENT		*/ NULL,
		/* DOM_NODE_DOCUMENT		*/ dom_rss_pop_document,
		/* DOM_NODE_DOCUMENT_TYPE	*/ NULL,
		/* DOM_NODE_DOCUMENT_FRAGMENT	*/ NULL,
		/* DOM_NODE_NOTATION		*/ NULL,
	}
};
