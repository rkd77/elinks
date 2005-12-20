/* The DOM tree navigation interface */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/dom/node.h"
#include "document/dom/stack.h"
#include "util/memory.h"
#include "util/string.h"


/* Navigator states */

#define DOM_STACK_STATE_GRANULARITY	0x7
#define DOM_STACK_CALLBACKS_SIZE	(sizeof(dom_stack_callback_T) * DOM_NODES)

static inline struct dom_stack_state *
realloc_dom_stack_states(struct dom_stack_state **states, size_t size)
{
	return mem_align_alloc(states, size, size + 1,
			       struct dom_stack_state,
			       DOM_STACK_STATE_GRANULARITY);
}

static inline unsigned char *
realloc_dom_stack_state_objects(struct dom_stack *stack)
{
#ifdef DEBUG_MEMLEAK
	return mem_align_alloc__(__FILE__, __LINE__, (void **) &stack->state_objects,
			       stack->depth, stack->depth + 1,
			       stack->object_size,
			       DOM_STACK_STATE_GRANULARITY);
#else
	return mem_align_alloc__((void **) &stack->state_objects,
			       stack->depth, stack->depth + 1,
			       stack->object_size,
			       DOM_STACK_STATE_GRANULARITY);
#endif
}

void
init_dom_stack(struct dom_stack *stack, void *data,
	       size_t object_size, int keep_nodes)
{
	assert(stack);

	memset(stack, 0, sizeof(*stack));

	stack->data        = data;
	stack->object_size = object_size;
	stack->keep_nodes  = !!keep_nodes;
}

void
done_dom_stack(struct dom_stack *stack)
{
	assert(stack);

	mem_free_if(stack->callbacks);
	mem_free_if(stack->states);
	mem_free_if(stack->state_objects);

	memset(stack, 0, sizeof(*stack));
}

void
add_dom_stack_callbacks(struct dom_stack *stack,
			struct dom_stack_callbacks *callbacks)
{
	struct dom_stack_callbacks **list;

	list = mem_realloc(stack->callbacks, sizeof(*list) * (stack->callbacks_size + 1));
	if (!list) return;

	stack->callbacks = list;
	stack->callbacks[stack->callbacks_size++] = callbacks;
}

enum dom_stack_action {
	DOM_STACK_PUSH,
	DOM_STACK_POP,
};

static void
call_dom_stack_callbacks(struct dom_stack *stack, struct dom_stack_state *state,
			 enum dom_stack_action action)
{
	/* FIME: Variable stack data, so the parse/selector/renderer/etc. can
	 * really work in parallel. */
	void *state_data = get_dom_stack_state_data(stack, state);
	int i;

	for (i = 0; i < stack->callbacks_size; i++) {
		dom_stack_callback_T callback;

		if (action == DOM_STACK_PUSH)
			callback = stack->callbacks[i]->push[state->node->type];
		else
			callback = stack->callbacks[i]->pop[state->node->type];

		if (callback)
			callback(stack, state->node, state_data);
	}
}

struct dom_node *
push_dom_node(struct dom_stack *stack, struct dom_node *node)
{
	struct dom_stack_state *state;

	assert(stack && node);
	assert(0 < node->type && node->type < DOM_NODES);

	if (stack->depth > DOM_STACK_MAX_DEPTH) {
		return NULL;
	}

	state = realloc_dom_stack_states(&stack->states, stack->depth);
	if (!state) {
		done_dom_node(node);
		return NULL;
	}

	state += stack->depth;

	if (stack->object_size) {
		unsigned char *state_objects;

		state_objects = realloc_dom_stack_state_objects(stack);
		if (!state_objects) {
			done_dom_node(node);
			return NULL;
		}

		state->depth = stack->depth;
	}

	state->node = node;

	/* Grow the state array to the new depth so the state accessors work
	 * in the callbacks */
	stack->depth++;
	call_dom_stack_callbacks(stack, state, DOM_STACK_PUSH);

	return node;
}

static int
do_pop_dom_node(struct dom_stack *stack, struct dom_stack_state *parent)
{
	struct dom_stack_state *state;

	assert(stack && !dom_stack_is_empty(stack));

	state = get_dom_stack_top(stack);
	if (state->immutable)
		return 1;

	call_dom_stack_callbacks(stack, state, DOM_STACK_POP);

	if (!stack->keep_nodes)
		done_dom_node(state->node);

	stack->depth--;
	assert(stack->depth >= 0);

	if (stack->object_size) {
		void *state_data = get_dom_stack_state_data(stack, state);

		memset(state_data, 0, stack->object_size);
	}

	memset(state, 0, sizeof(*state));

	return state == parent;
}

void
pop_dom_node(struct dom_stack *stack)
{
	assert(stack);

	if (dom_stack_is_empty(stack)) return;

	do_pop_dom_node(stack, get_dom_stack_parent(stack));
}

void
pop_dom_nodes(struct dom_stack *stack, enum dom_node_type type,
	      struct dom_string *string)
{
	struct dom_stack_state *state;

	assert(stack);

	if (dom_stack_is_empty(stack)) return;

	state = search_dom_stack(stack, type, string);
	if (state)
		pop_dom_state(stack, state);
}

void
pop_dom_state(struct dom_stack *stack, struct dom_stack_state *target)
{
	struct dom_stack_state *state;
	unsigned int pos;

	assert(stack);

	if (!target) return;

	if (dom_stack_is_empty(stack)) return;

	foreachback_dom_stack_state (stack, state, pos) {
		if (do_pop_dom_node(stack, target))
			break;;
	}
}

void
walk_dom_nodes(struct dom_stack *stack, struct dom_node *root)
{
	assert(root && stack);

	push_dom_node(stack, root);

	while (!dom_stack_is_empty(stack)) {
		struct dom_stack_state *state = get_dom_stack_top(stack);
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

			if (push_dom_node(stack, child))
				continue;
		}

		pop_dom_node(stack);
	}
}
