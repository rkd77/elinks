
#ifndef EL__DOCUMENT_DOM_STACK_H
#define EL__DOCUMENT_DOM_STACK_H

#include "document/document.h"
#include "document/dom/node.h"
#include "util/error.h"
#include "util/hash.h"

struct dom_stack;

enum dom_exception_code {
	DOM_ERR_NONE				=  0,
	DOM_ERR_INDEX_SIZE			=  1,
	DOM_ERR_STRING_SIZE			=  2,
	DOM_ERR_HIERARCHY_REQUEST		=  3,
	DOM_ERR_WRONG_DOCUMENT			=  4,
	DOM_ERR_INVALID_CHARACTER		=  5,
	DOM_ERR_NO_DATA_ALLOWED			=  6,
	DOM_ERR_NO_MODIFICATION_ALLOWED		=  7,
	DOM_ERR_NOT_FOUND			=  8,
	DOM_ERR_NOT_SUPPORTED			=  9,
	DOM_ERR_INUSE_ATTRIBUTE			= 10,

	/* Introduced in DOM Level 2: */
	DOM_ERR_INVALID_STATE			= 11,
	DOM_ERR_SYNTAX				= 12,
	DOM_ERR_INVALID_MODIFICATION		= 13,
	DOM_ERR_NAMESPACE			= 14,
	DOM_ERR_INVALID_ACCESS			= 15,
};

typedef struct dom_node *
	(*dom_stack_callback_T)(struct dom_stack *, struct dom_node *, void *);

#define DOM_STACK_MAX_DEPTH	4096

struct dom_stack_state {
	struct dom_node *node;

	struct dom_node_list *list;
	size_t index;

	dom_stack_callback_T callback;
	void *data;
};

/* The DOM stack is a convenient way to traverse DOM trees. Also it
 * maintains needed state info and is therefore also a holder of the current
 * context since the stack is used to when the DOM tree is manipulated. */
struct dom_stack {
	/* The stack of nodes */
	struct dom_stack_state *states;
	size_t depth;

	unsigned char *state_objects;
	size_t object_size;

	/* If some operation failed this member will signal the error code */
	enum dom_exception_code exception;

	/* Parser and document specific stuff */
	dom_stack_callback_T callbacks[DOM_NODES];
	void *data;
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
void init_dom_stack(struct dom_stack *stack, void *data,
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
