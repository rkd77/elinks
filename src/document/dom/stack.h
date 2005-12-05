
#ifndef EL__DOCUMENT_DOM_STACK_H
#define EL__DOCUMENT_DOM_STACK_H

#include "document/document.h"
#include "document/dom/node.h"
#include "util/error.h"
#include "util/hash.h"

struct dom_stack;

typedef struct dom_node *
	(*dom_stack_callback_T)(struct dom_stack *, struct dom_node *, void *);

#define DOM_STACK_MAX_DEPTH	4096

struct dom_stack_state {
	struct dom_node *node;

	/* Used for recording which node list are currently being 'decended'
	 * into. E.g. whether we are iterating all child elements or attributes
	 * of an element. */
	struct dom_node_list *list;
	/* The index (in the list above) which are currently being handled. */
	size_t index;

	/* A callback registered to be called when the node is popped. Used for
	 * correctly highlighting ending elements (e.g. </a>). */
	dom_stack_callback_T callback;

	/* The depth of the state in the stack. This is amongst other things
	 * used to get the state object data. */
	unsigned int depth;
};

/* The DOM stack is a convenient way to traverse DOM trees. Also it
 * maintains needed state info and is therefore also a holder of the current
 * context since the stack is used to when the DOM tree is manipulated. */
struct dom_stack {
	/* The stack of nodes */
	struct dom_stack_state *states;
	size_t depth;

	/* This is one big array of parser specific objects. */
	/* The objects hold parser specific data. For the SGML parser this
	 * holds DTD-oriented info about the node (recorded in struct
	 * sgml_node_info). E.g.  whether an element node is optional. */
	unsigned char *state_objects;
	size_t object_size;

	/* Parser and document specific stuff */
	dom_stack_callback_T callbacks[DOM_NODES];
	void *renderer;

	void *parser;
};

#define dom_stack_has_parents(nav) \
	((nav)->states && (nav)->depth > 0)

static inline struct dom_stack_state *
get_dom_stack_state(struct dom_stack *stack, int top_offset)
{
	assertm(stack->depth - 1 - top_offset >= 0,
		"Attempting to access invalid state");
	return &stack->states[stack->depth - 1 - top_offset];
}

#define get_dom_stack_parent(nav)	get_dom_stack_state(nav, 1)
#define get_dom_stack_top(nav)		get_dom_stack_state(nav, 0)

#define get_dom_stack_state_data(stack, state) \
	((void *) &(stack)->state_objects[(state)->depth * (stack)->object_size])

/* The state iterators do not include the bottom state */

#define foreach_dom_state(nav, item, pos)			\
	for ((pos) = 1; (pos) < (nav)->depth; (pos)++)		\
		if (((item) = &(nav)->states[(pos)]))

#define foreachback_dom_state(nav, item, pos)			\
	for ((pos) = (nav)->depth - 1; (pos) > 0; (pos)--)	\
		if (((item) = &(nav)->states[(pos)]))

/* Dive through the stack states in search for the specified match. */
static inline struct dom_stack_state *
search_dom_stack(struct dom_stack *stack, enum dom_node_type type,
		 unsigned char *string, uint16_t length)
{
	struct dom_stack_state *state;
	int pos;

	/* FIXME: Take node subtype and compare if non-zero or something. */
	foreachback_dom_state (stack, state, pos) {
		struct dom_node *parent = state->node;

		if (parent->type == type
		    && parent->length == length
		    && !strncasecmp(parent->string, string, length))
			return state;
	}

	return NULL;
}


/* Life cycle functions. */

/* The @object_size arg tells whether the stack should allocate objects for each
 * state to be assigned to the state's @data member. Zero means no state data should
 * be allocated. */
void init_dom_stack(struct dom_stack *stack, void *parser, void *renderer,
		    dom_stack_callback_T callbacks[DOM_NODES],
		    size_t object_size);
void done_dom_stack(struct dom_stack *stack);

/* Decends down to the given node making it the current parent */
/* If an error occurs the node is free()d and NULL is returned */
struct dom_node *push_dom_node(struct dom_stack *stack, struct dom_node *node);

/* Ascends the stack to the current parent */
void pop_dom_node(struct dom_stack *stack);

/* Ascends the stack looking for specific parent */
void pop_dom_nodes(struct dom_stack *stack, enum dom_node_type type,
		   unsigned char *string, uint16_t length);

/* Visit each node in the tree rooted at @root pre-order */
void walk_dom_nodes(struct dom_stack *stack, struct dom_node *root);

#endif
