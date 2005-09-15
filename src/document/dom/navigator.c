/* The DOM tree navigation interface */
/* $Id: navigator.c,v 1.9 2005/03/30 20:57:28 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/dom/navigator.h"
#include "document/dom/node.h"
#include "util/memory.h"
#include "util/string.h"


/* Navigator states */

#define DOM_NAVIGATOR_STATE_GRANULARITY 0x7
#define DOM_NAVIGATOR_CALLBACKS_SIZE	(sizeof(dom_navigator_callback_T) * DOM_NODES)

static inline struct dom_navigator_state *
realloc_dom_navigator_states(struct dom_navigator_state **states, size_t size)
{
	return mem_align_alloc(states, size, size + 1,
			       struct dom_navigator_state,
			       DOM_NAVIGATOR_STATE_GRANULARITY);
}

static inline unsigned char *
realloc_dom_navigator_state_objects(struct dom_navigator *navigator)
{
#ifdef DEBUG_MEMLEAK
	return mem_align_alloc__(__FILE__, __LINE__, (void **) &navigator->state_objects,
			       navigator->depth, navigator->depth + 1,
			       navigator->object_size,
			       DOM_NAVIGATOR_STATE_GRANULARITY);
#else
	return mem_align_alloc__((void **) &navigator->state_objects,
			       navigator->depth, navigator->depth + 1,
			       navigator->object_size,
			       DOM_NAVIGATOR_STATE_GRANULARITY);
#endif
}

void
init_dom_navigator(struct dom_navigator *navigator, void *data,
		   dom_navigator_callback_T callbacks[DOM_NODES],
		   size_t object_size)
{
	assert(navigator);

	memset(navigator, 0, sizeof(*navigator));

	navigator->data        = data;
	navigator->object_size = object_size;

	if (callbacks)
		memcpy(navigator->callbacks, callbacks, DOM_NAVIGATOR_CALLBACKS_SIZE);
}

void
done_dom_navigator(struct dom_navigator *navigator)
{
	assert(navigator);

	mem_free_if(navigator->states);
	mem_free_if(navigator->state_objects);

	memset(navigator, 0, sizeof(*navigator));
}

struct dom_node *
push_dom_node(struct dom_navigator *navigator, struct dom_node *node)
{
	dom_navigator_callback_T callback;
	struct dom_navigator_state *state;

	assert(navigator && node);
	assert(0 < node->type && node->type < DOM_NODES);

	if (navigator->depth > DOM_NAVIGATOR_MAX_DEPTH) {
		return NULL;
	}

	state = realloc_dom_navigator_states(&navigator->states, navigator->depth);
	if (!state) {
		done_dom_node(node);
		return NULL;
	}

	state += navigator->depth;

	if (navigator->object_size) {
		unsigned char *state_objects;
		size_t offset = navigator->depth * navigator->object_size;

		state_objects = realloc_dom_navigator_state_objects(navigator);
		if (!state_objects) {
			done_dom_node(node);
			return NULL;
		}

		state->data = (void *) &state_objects[offset];
	}

	state->node = node;

	/* Grow the state array to the new depth so the state accessors work
	 * in the callbacks */
	navigator->depth++;

	callback = navigator->callbacks[node->type];
	if (callback) {
		node = callback(navigator, node, state->data);

		/* If the callback returned NULL pop the state immediately */
		if (!node) {
			memset(state, 0, sizeof(*state));
			navigator->depth--;
			assert(navigator->depth >= 0);
		}
	}

	return node;
}

static int
do_pop_dom_node(struct dom_navigator *navigator, struct dom_navigator_state *parent)
{
	struct dom_navigator_state *state;

	assert(navigator);
	if (!dom_navigator_has_parents(navigator)) return 0;

	state = get_dom_navigator_top(navigator);
	if (state->callback) {
		/* Pass the node we are popping to and _not_ the state->node */
		state->callback(navigator, parent->node, state->data);
	}

	navigator->depth--;
	assert(navigator->depth >= 0);

	if (navigator->object_size && state->data) {
		size_t offset = navigator->depth * navigator->object_size;

		/* I tried to use item->data here but it caused a memory
		 * corruption bug on fm. This is also less trustworthy in that
		 * the state->data pointer could have been mangled. --jonas */
		memset(&navigator->state_objects[offset], 0, navigator->object_size);
	}

	memset(state, 0, sizeof(*state));

	return state == parent;
}

void
pop_dom_node(struct dom_navigator *navigator)
{
	assert(navigator);
	if (!dom_navigator_has_parents(navigator)) return;

	do_pop_dom_node(navigator, get_dom_navigator_parent(navigator));
}

void
pop_dom_nodes(struct dom_navigator *navigator, enum dom_node_type type,
	      unsigned char *string, uint16_t length)
{
	struct dom_navigator_state *state, *parent;
	unsigned int pos;

	if (!dom_navigator_has_parents(navigator)) return;

	parent = search_dom_navigator(navigator, type, string, length);
	if (!parent) return;

	foreachback_dom_state (navigator, state, pos) {
		if (do_pop_dom_node(navigator, parent))
			break;;
	}
}

void
walk_dom_nodes(struct dom_navigator *navigator, struct dom_node *root)
{
	assert(root && navigator);

	push_dom_node(navigator, root);

	while (dom_navigator_has_parents(navigator)) {
		struct dom_navigator_state *state = get_dom_navigator_top(navigator);
		struct dom_node_list *list = state->list;
		struct dom_node *node = state->node;

		switch (node->type) {
		case DOM_NODE_DOCUMENT:
			if (!list) list = node->data.document.children;
			break;

		case DOM_NODE_ELEMENT:
			if (!list) list = node->data.element.map;

			if (list == node->data.element.children) break;
			if (is_dom_node_list_member(list, state->index)
			    && list == node->data.element.map)
				break;

			list = node->data.element.children;
			break;

		case DOM_NODE_PROCESSING_INSTRUCTION:
			if (!list) list = node->data.proc_instruction.map;
			break;

		case DOM_NODE_DOCUMENT_TYPE:
			if (!list) list = node->data.document_type.entities;

			if (list == node->data.document_type.notations) break;
			if (is_dom_node_list_member(list, state->index)
			    && list == node->data.document_type.entities)
				break;

			list = node->data.document_type.notations;
			break;

		case DOM_NODE_ATTRIBUTE:
		case DOM_NODE_TEXT:
		case DOM_NODE_CDATA_SECTION:
		case DOM_NODE_COMMENT:
		case DOM_NODE_NOTATION:
		case DOM_NODE_DOCUMENT_FRAGMENT:
		case DOM_NODE_ENTITY_REFERENCE:
		case DOM_NODE_ENTITY:
		default:
			break;
		}

		/* Reset list state if it is a new list */
		if (list != state->list) {
			state->list  = list;
			state->index = 0;
		}

		/* If we have next child node */
		if (is_dom_node_list_member(list, state->index)) {
			struct dom_node *child = list->entries[state->index++];

			if (push_dom_node(navigator, child))
				continue;
		}

		pop_dom_node(navigator);
	}
}
