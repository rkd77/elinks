#ifndef EL__DOCUMENT_DOM_STACK_H
#define EL__DOCUMENT_DOM_STACK_H

#include "document/document.h"
#include "document/dom/node.h"
#include "util/error.h"
#include "util/hash.h"

struct dom_stack;

typedef struct dom_node *
	(*dom_stack_callback_T)(struct dom_stack *, struct dom_node *, void *);

struct dom_stack_callbacks {
	dom_stack_callback_T push[DOM_NODES];
	dom_stack_callback_T pop[DOM_NODES];
};

#define DOM_STACK_MAX_DEPTH	4096

struct dom_stack_state {
	struct dom_node *node;

	/* Used for recording which node list are currently being 'decended'
	 * into. E.g. whether we are iterating all child elements or attributes
	 * of an element. */
	struct dom_node_list *list;
	/* The index (in the list above) which are currently being handled. */
	size_t index;

	/* The depth of the state in the stack. This is amongst other things
	 * used to get the state object data. */
	unsigned int depth;

	/* Wether this stack state can be popped with pop_dom_*() family. */
	unsigned int immutable:1;
};

/* The DOM stack is a convenient way to traverse DOM trees. Also it
 * maintains needed state info and is therefore also a holder of the current
 * context since the stack is used to when the DOM tree is manipulated. */
struct dom_stack {
	/* The stack of nodes */
	struct dom_stack_state *states;
	size_t depth;

	/* Keep nodes when popping them or call done_dom_node() on them. */
	unsigned int keep_nodes:1;

	/* This is one big array of parser specific objects. */
	/* The objects hold parser specific data. For the SGML parser this
	 * holds DTD-oriented info about the node (recorded in struct
	 * sgml_node_info). E.g.  whether an element node is optional. */
	unsigned char *state_objects;
	size_t object_size;

	/* Callbacks which should be called for the pushed and popped nodes. */
	struct dom_stack_callbacks *callbacks;

	/* Data specific to the parser and renderer. */
	void *data;
};

#define dom_stack_is_empty(stack) \
	(!(stack)->states || (stack)->depth == 0)

static inline struct dom_stack_state *
get_dom_stack_state(struct dom_stack *stack, int top_offset)
{
	assertm(stack->depth - 1 - top_offset >= 0,
		"Attempting to access invalid state");
	return &stack->states[stack->depth - 1 - top_offset];
}

#define get_dom_stack_parent(stack)	get_dom_stack_state(stack, 1)
#define get_dom_stack_top(stack)	get_dom_stack_state(stack, 0)

#define get_dom_stack_state_data(stack, state) \
	((void *) &(stack)->state_objects[(state)->depth * (stack)->object_size])

/* The state iterators do not include the bottom state */

#define foreach_dom_stack_state(stack, state, pos)			\
	for ((pos) = 0; (pos) < (stack)->depth; (pos)++)		\
		if (((state) = &(stack)->states[(pos)]))

#define foreachback_dom_stack_state(stack, state, pos)			\
	for ((pos) = (stack)->depth - 1; (pos) >= 0; (pos)--)		\
		if (((state) = &(stack)->states[(pos)]))

/* Dive through the stack states in search for the specified match. */
static inline struct dom_stack_state *
search_dom_stack(struct dom_stack *stack, enum dom_node_type type,
		 struct dom_string *string)
{
	struct dom_stack_state *state;
	int pos;

	/* FIXME: Take node subtype and compare if non-zero or something. */
	foreachback_dom_stack_state (stack, state, pos) {
		struct dom_node *parent = state->node;

		if (parent->type == type
		    && parent->string.length == string->length
		    && !strncasecmp(parent->string.string, string->string, string->length))
			return state;
	}

	return NULL;
}


/* Life cycle functions. */

/* The @object_size arg tells whether the stack should allocate objects for each
 * state to be assigned to the state's @data member. Zero means no state data should
 * be allocated. */
void init_dom_stack(struct dom_stack *stack, void *data,
		    struct dom_stack_callbacks *callbacks,
		    size_t object_size, int keep_nodes);
void done_dom_stack(struct dom_stack *stack);

/* Decends down to the given node making it the current parent */
/* If an error occurs the node is free()d and NULL is returned */
struct dom_node *push_dom_node(struct dom_stack *stack, struct dom_node *node);

/* Ascends the stack to the current parent */
void pop_dom_node(struct dom_stack *stack);

/* Ascends the stack looking for specific parent */
void pop_dom_nodes(struct dom_stack *stack, enum dom_node_type type,
		   struct dom_string *string);

/* Pop all stack states until a specific state is reached. */
void pop_dom_state(struct dom_stack *stack, struct dom_stack_state *target);

/* Visit each node in the tree rooted at @root pre-order */
void walk_dom_nodes(struct dom_stack *stack, struct dom_node *root);

#endif
