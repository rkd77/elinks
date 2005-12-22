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

static inline struct dom_stack_state *
realloc_dom_stack_context(struct dom_stack_context **contexts, size_t size)
{
	return mem_align_alloc(contexts, size, size + 1,
			       struct dom_stack_context,
			       DOM_STACK_STATE_GRANULARITY);
}

static inline unsigned char *
realloc_dom_stack_state_objects(struct dom_stack_context *context, size_t depth)
{
#ifdef DEBUG_MEMLEAK
	return mem_align_alloc__(__FILE__, __LINE__, (void **) &context->state_objects,
			       depth, depth + 1,
			       context->info->object_size,
			       DOM_STACK_STATE_GRANULARITY);
#else
	return mem_align_alloc__((void **) &context->state_objects,
			       depth, depth + 1,
			       context->info->object_size,
			       DOM_STACK_STATE_GRANULARITY);
#endif
}

void
init_dom_stack(struct dom_stack *stack, enum dom_stack_flag flags)
{
	assert(stack);

	memset(stack, 0, sizeof(*stack));

	stack->flags = flags;
}

void
done_dom_stack(struct dom_stack *stack)
{
	int i;

	assert(stack);

	for (i = 0; i < stack->contexts_size; i++) {
		mem_free_if(stack->contexts[i].state_objects);
	}

	mem_free_if(stack->contexts);
	mem_free_if(stack->states);

	memset(stack, 0, sizeof(*stack));
}

void
add_dom_stack_context(struct dom_stack *stack, void *data,
		      struct dom_stack_context_info *context_info)
{
	struct dom_stack_context *context;

	if (!realloc_dom_stack_context(&stack->contexts, stack->contexts_size))
		return;

	context = &stack->contexts[stack->contexts_size++];
	context->info = context_info;
	context->data = data;
}

enum dom_stack_action {
	DOM_STACK_PUSH,
	DOM_STACK_POP,
};

static void
call_dom_stack_callbacks(struct dom_stack *stack, struct dom_stack_state *state,
			 enum dom_stack_action action)
{
	int i;

	for (i = 0; i < stack->contexts_size; i++) {
		struct dom_stack_context *context = &stack->contexts[i];
		void *state_data = get_dom_stack_state_data(context, state);
		dom_stack_callback_T callback;

		if (action == DOM_STACK_PUSH)
			callback = context->info->push[state->node->type];
		else
			callback = context->info->pop[state->node->type];

		if (callback) {
			stack->current = context;
			callback(stack, state->node, state_data);
			stack->current = NULL;
		}
	}
}

struct dom_node *
push_dom_node(struct dom_stack *stack, struct dom_node *node)
{
	struct dom_stack_state *state;
	int i;

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

	for (i = 0; i < stack->contexts_size; i++) {
		struct dom_stack_context *context = &stack->contexts[i];

		if (context->info->object_size
		    && !realloc_dom_stack_state_objects(context, stack->depth)) {
			done_dom_node(node);
			return NULL;
		}
	}

	state->depth = stack->depth;
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
	int i;

	assert(stack && !dom_stack_is_empty(stack));

	state = get_dom_stack_top(stack);
	if (state->immutable)
		return 1;

	call_dom_stack_callbacks(stack, state, DOM_STACK_POP);

	if (!(stack->flags & DOM_STACK_KEEP_NODES))
		done_dom_node(state->node);

	stack->depth--;
	assert(stack->depth >= 0);

	for (i = 0; i < stack->contexts_size; i++) {
		struct dom_stack_context *context = &stack->contexts[i];

		if (context->info->object_size) {
			void *state_data = get_dom_stack_state_data(context, state);

			memset(state_data, 0, context->info->object_size);
		}
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

struct dom_stack_state *
search_dom_stack(struct dom_stack *stack, enum dom_node_type type,
		 struct dom_string *string)
{
	struct dom_stack_state *state;
	int pos;

	/* FIXME: Take node subtype and compare if non-zero or something. */
	foreachback_dom_stack_state (stack, state, pos) {
		struct dom_node *parent = state->node;

		if (parent->type == type
		    && !dom_string_casecmp(&parent->string, string))
			return state;
	}

	return NULL;
}

/* FIXME: Instead of walking all nodes in the tree only visit those which are
 * of actual interest to the contexts on the stack. */
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


/* DOM Stack Tracing: */

#ifdef DOM_STACK_TRACE

/* Compress a string to a single line with newlines etc. replaced with "\\n"
 * sequence. */
static inline unsigned char *
compress_string(unsigned char *string, unsigned int length)
{
	struct string buffer;
	unsigned char escape[2] = "\\";

	if (!init_string(&buffer)) return NULL;

	for (; length > 0; string++, length--) {
		unsigned char *bytes = string;

		if (*string == '\n' || *string == '\r' || *string == '\t') {
			bytes	  = escape;
			escape[1] = *string == '\n' ? 'n'
				  : (*string == '\r' ? 'r' : 't');
		}

		add_bytes_to_string(&buffer, bytes, bytes == escape ? 2 : 1);
	}

	return buffer.source;
}

/* Set @string to the value of the given @node, however, with strings
 * compressed and entity references 'expanded'. */
static void
set_enhanced_dom_node_value(struct dom_string *string, struct dom_node *node)
{
	struct dom_string *value;

	assert(node);

	memset(string, 0, sizeof(*string));

	switch (node->type) {
	case DOM_NODE_ENTITY_REFERENCE:
		/* XXX: The ASCII codepage is hardcoded here since we do not
		 * want to depend on anything and this is really just for
		 * debugging. */
		string->string = get_entity_string(node->string.string,
						   node->string.length, 0);
		string->string = null_or_stracpy(string->string);
		break;

	default:
		value = get_dom_node_value(node);
		if (!value) {
			set_dom_string(string, NULL, 0);
			return;
		}

		string->string = compress_string(value->string, value->length);
	}

	string->length = string->string ? strlen(string->string) : 0;
}

static unsigned char indent_string[] =
	"                                                                    ";

#define get_indent_offset(stack) \
	((stack)->depth < sizeof(indent_string)/2 ? (stack)->depth * 2 : sizeof(indent_string))

static void
dom_stack_trace_tree(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_string *value = &node->string;
	struct dom_string *name = get_dom_node_name(node);

	LOG_INFO("%.*s %.*s: %.*s",
		get_indent_offset(stack), indent_string,
		name->length, name->string,
		value->length, value->string);
}

static void
dom_stack_trace_id_leaf(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_string value;
	struct dom_string *name;
	struct dom_string *id;

	assert(node);

	name	= get_dom_node_name(node);
	id	= get_dom_node_type_name(node->type);
	set_enhanced_dom_node_value(&value, node);

	LOG_INFO("%.*s %.*s: %.*s -> %.*s",
		get_indent_offset(stack), indent_string,
		id->length, id->string, name->length, name->string,
		value.length, value.string);

	if (is_dom_string_set(&value))
		done_dom_string(&value);
}

static void
dom_stack_trace_leaf(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_string *name;
	struct dom_string value;

	assert(node);

	name	= get_dom_node_name(node);
	set_enhanced_dom_node_value(&value, node);

	LOG_INFO("%.*s %.*s: %.*s",
		get_indent_offset(stack), indent_string,
		name->length, name->string,
		value.length, value.string);

	if (is_dom_string_set(&value))
		done_dom_string(&value);
}

static void
dom_stack_trace_branch(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_string *name;
	struct dom_string *id;

	assert(node);

	name	= get_dom_node_name(node);
	id	= get_dom_node_type_name(node->type);

	LOG_INFO("%.*s %.*s: %.*s",
		get_indent_offset(stack), indent_string,
		id->length, id->string, name->length, name->string);
}

struct dom_stack_context_info dom_stack_trace_context_info = {
	/* Object size: */			0,
	/* Push: */
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ dom_stack_trace_branch,
		/* DOM_NODE_ATTRIBUTE		*/ dom_stack_trace_id_leaf,
		/* DOM_NODE_TEXT		*/ dom_stack_trace_leaf,
		/* DOM_NODE_CDATA_SECTION	*/ dom_stack_trace_id_leaf,
		/* DOM_NODE_ENTITY_REFERENCE	*/ dom_stack_trace_id_leaf,
		/* DOM_NODE_ENTITY		*/ dom_stack_trace_id_leaf,
		/* DOM_NODE_PROC_INSTRUCTION	*/ dom_stack_trace_id_leaf,
		/* DOM_NODE_COMMENT		*/ dom_stack_trace_leaf,
		/* DOM_NODE_DOCUMENT		*/ dom_stack_trace,
		/* DOM_NODE_DOCUMENT_TYPE	*/ dom_stack_trace_id_leaf,
		/* DOM_NODE_DOCUMENT_FRAGMENT	*/ dom_stack_trace_id_leaf,
		/* DOM_NODE_NOTATION		*/ dom_stack_trace_id_leaf,
	},
	/* Pop: */
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ NULL,
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
	}
};

#endif /* DOM_STACK_TRACE */
