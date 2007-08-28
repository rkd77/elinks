/* DOM-based RSS renderer */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/document.h"
#include "document/dom/util.h"
#include "document/dom/rss.h"
#include "dom/sgml/rss/rss.h"
#include "dom/node.h"
#include "dom/stack.h"
#include "intl/charsets.h"
#include "util/error.h"
#include "util/memory.h"


/* DOM RSS Renderer */


static enum dom_code
dom_rss_push_element(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_renderer *renderer = stack->current->data;

	assert(node && renderer && renderer->document);

	switch (node->data.element.type) {
	case RSS_ELEMENT_CHANNEL:
		/* The stack should have: #document * channel */
		if (stack->depth != 3)
			break;

		if (!renderer->channel) {
			renderer->channel = node;
		}
		break;

	case RSS_ELEMENT_ITEM:
		/* The stack should have: #document * channel item */
#if 0
		/* Don't be so strict ... */
		if (stack->depth != 4)
			break;
#endif
		/* ... but be exclusive. */
		if (renderer->item)
			break;
		add_to_dom_node_list(&renderer->items, node, -1);
		renderer->item = node;
		break;

	case RSS_ELEMENT_LINK:
	case RSS_ELEMENT_DESCRIPTION:
	case RSS_ELEMENT_TITLE:
	case RSS_ELEMENT_AUTHOR:
	case RSS_ELEMENT_PUBDATE:
		if (!node->parent || renderer->node != node->parent)
			break;

		renderer->node = node;
	}

	return DOM_CODE_OK;
}

static enum dom_code
dom_rss_pop_element(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_renderer *renderer = stack->current->data;
	struct dom_node_list **list;

	assert(node && renderer && renderer->document);

	switch (node->data.element.type) {
	case RSS_ELEMENT_ITEM:
		if (is_dom_string_set(&renderer->text))
			done_dom_string(&renderer->text);
		renderer->item = NULL;
		break;

	case RSS_ELEMENT_LINK:
	case RSS_ELEMENT_DESCRIPTION:
	case RSS_ELEMENT_TITLE:
	case RSS_ELEMENT_AUTHOR:
	case RSS_ELEMENT_PUBDATE:
		if (!is_dom_string_set(&renderer->text)
		    || !node->parent
		    || renderer->item != node->parent
		    || renderer->node != node)
			break;

		/* Replace any child nodes with the normalized text node. */
		list = get_dom_node_list(node->parent, node);
		done_dom_node_list(*list);
		if (is_dom_string_set(&renderer->text)) {
			if (!add_dom_node(node, DOM_NODE_TEXT, &renderer->text))
				done_dom_string(&renderer->text);
		}
		renderer->node = NULL;
		break;

	default:
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
	struct dom_string *title  = get_rss_text(item, RSS_ELEMENT_TITLE);
	struct dom_string *link   = get_rss_text(item, RSS_ELEMENT_LINK);
	struct dom_string *author = get_rss_text(item, RSS_ELEMENT_AUTHOR);
	struct dom_string *date   = get_rss_text(item, RSS_ELEMENT_PUBDATE);

	if (title && is_dom_string_set(title)) {
		if (item == renderer->channel) {
			unsigned char *str;

			str = convert_string(renderer->convert_table,
					     title->string, title->length,
					     renderer->document->options.cp,
					     CSM_DEFAULT, NULL, NULL, NULL);
			if (str)
				renderer->document->title = str;
		}
		render_dom_text(renderer, &renderer->styles[DOM_NODE_ELEMENT],
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
		render_dom_text(renderer, &renderer->styles[DOM_NODE_COMMENT],
				author->string, author->length);
	}

	if (date && is_dom_string_set(date)) {
		if (author && is_dom_string_set(author)) {
			render_dom_text(renderer, &renderer->styles[DOM_NODE_COMMENT],
					" - ", 3);
		}

		render_dom_text(renderer, &renderer->styles[DOM_NODE_COMMENT],
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
dom_rss_pop_document(struct dom_stack *stack, struct dom_node *root, void *data)
{
	struct dom_renderer *renderer = stack->current->data;

	if (!renderer->channel)
		return DOM_CODE_OK;

	render_rss_item(renderer, renderer->channel);

	if (renderer->items) {
		struct dom_node *node;
		int index;

		foreach_dom_node (renderer->items, node, index) {
			Y(renderer)++;
			X(renderer) = 0;
			render_rss_item(renderer, node);
		}
	}

	if (is_dom_string_set(&renderer->text))
		done_dom_string(&renderer->text);
	mem_free_if(renderer->items);

	done_dom_node(root);

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
		/* DOM_NODE_DOCUMENT		*/ NULL,
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
