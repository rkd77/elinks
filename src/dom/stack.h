#ifndef EL_DOM_STACK_H
#define EL_DOM_STACK_H

#include "dom/node.h"
#include "util/error.h"
#include "util/hash.h"

struct dom_stack;

/* API Doc :: dom-stack */

/** DOM stack callback
 *
 * Used by contexts, for 'hooking' into the node traversing. */
typedef void (*dom_stack_callback_T)(struct dom_stack *, struct dom_node *, void *);

#define DOM_STACK_MAX_DEPTH	4096

/** DOM stack state
 *
 * This state records what node and where it is placed. */
struct dom_stack_state {
	/** The node assiciated with the state */
	struct dom_node *node;
	/**
	 * The depth of the state in the stack. This is amongst other things
	 * used to get the state object data. */
	unsigned int depth;
	/** Whether this stack state can be popped with pop_dom_*() family. */
	unsigned int immutable:1;
};

/** DOM stack context info
 *
 * To add a DOM stack context define this struct either statically or on the
 * stack. */
struct dom_stack_context_info {
	/**
	 * This member tells whether the stack should allocate objects for each
	 * state to be assigned to the state's @data member.  Zero means no
	 * state data should be allocated. */
	size_t object_size;

	/** Callbacks to be called when pushing nodes. */
	dom_stack_callback_T push[DOM_NODES];
	/** Callbacks to be called when popping nodes. */
	dom_stack_callback_T pop[DOM_NODES];
};

/** DOM stack context
 *
 * This holds 'runtime' data for the stack context. */
struct dom_stack_context {
	/** Data specific to the context. */
	void *data;

	/**
	 * This is one big array of context specific objects. For the SGML
	 * parser this holds DTD-oriented info about the node (recorded in
	 * struct sgml_node_info). E.g.  whether an element node is optional.
	 */
	unsigned char *state_objects;

	/** Info about node callbacks and such. */
	struct dom_stack_context_info *info;
};

/** Flags for controlling the DOM stack */
enum dom_stack_flag {
	/** No flag needed. */
	DOM_STACK_FLAG_NONE = 0,

	/** Free nodes when popping by calling ref:[done_dom_node]. */
	DOM_STACK_FLAG_FREE_NODES = 1,
};

/** The DOM stack
 *
 * The DOM stack is a convenient way to traverse DOM trees. Also it
 * maintains needed state info and is therefore also a holder of the current
 * context since the stack is used to when the DOM tree is manipulated. */
struct dom_stack {
	/** The states currently on the stack. */
	struct dom_stack_state *states;
	/** The depth of the stack. */
	size_t depth;

	/** Flags given to ref:[init_dom_stack]. */
	enum dom_stack_flag flags;

	/** Contexts for the pushed and popped nodes. */
	struct dom_stack_context **contexts;
	/** The number of active contexts. */
	size_t contexts_size;

	/**
	 * The current context. Only meaningful within
	 * ref:[dom_stack_callback_T] functions. */
	struct dom_stack_context *current;
};

/** Check whether stack is empty or not
 *
 * stack::	The stack to check.
 *
 * Returns non-zero if stack is empty. */
#define dom_stack_is_empty(stack) \
	(!(stack)->states || (stack)->depth == 0)

/** Access state by offset from top
 *
 * stack::	The stack to fetch the state from.
 * top_offset::	The offset from the stack top, zero is the top.
 *
 * Returns the requested state. */
static inline struct dom_stack_state *
get_dom_stack_state(struct dom_stack *stack, int top_offset)
{
	assertm(stack->depth - 1 - top_offset >= 0,
		"Attempting to access invalid state");
	return &stack->states[stack->depth - 1 - top_offset];
}

/** Access the stack top
 *
 * stack:: The stack to get the top state from.
 *
 * Returns the top state. */
#define get_dom_stack_top(stack) \
	get_dom_stack_state(stack, 0)

/** Access context specific state data
 *
 * Similar to ref:[get_dom_stack_state], this will fetch the data
 * associated with the state for the given context.
 *
 * context::	The context to get data from.
 * state::	The stack state to get data from.
 *
 * Returns the state data or NULL if ref:[dom_stack_context_info.object_size]
 * was zero. */
static inline void *
get_dom_stack_state_data(struct dom_stack_context *context,
			 struct dom_stack_state *state)
{
	size_t object_size = context->info->object_size;

	if (!object_size) return NULL;

	assertm(context->state_objects);

	return (void *) &context->state_objects[state->depth * object_size];
}

/*#define DOM_STACK_TRACE*/

#ifdef DOM_STACK_TRACE
extern struct dom_stack_context_info dom_stack_trace_context_info;
/** Get debug info from the DOM stack
 *
 * Define `DOM_STACK_TRACE` to have debug info about the nodes added printed to
 * the log. It will define add_dom_stack_tracer() to not be a no-op.
 *
 * Run as:
 *
 *	ELINKS_LOG=/tmp/dom-dump.txt ./elinks -no-connect <url>
 *
 * to have the debug dumped into a file. */
#define add_dom_stack_tracer(stack, name) \
	add_dom_stack_context(stack, name, &dom_stack_trace_context_info)
#else
#define add_dom_stack_tracer(stack, name) /* Nada */
#endif

/** The state iterators
 *
 * To safely iterate through the stack state iterators. */

/** Iterate the stack from bottom to top. */
#define foreach_dom_stack_state(stack, state, pos)			\
	for ((pos) = 0; (pos) < (stack)->depth; (pos)++)		\
		if (((state) = &(stack)->states[(pos)]))

/** Iterate the stack from top to bottom. */
#define foreachback_dom_stack_state(stack, state, pos)			\
	for ((pos) = (stack)->depth - 1; (pos) >= 0; (pos)--)		\
		if (((state) = &(stack)->states[(pos)]))


/* Life cycle functions. */

/** Initialise a DOM stack
 * stack::	Pointer to a (preallocated) stack.
 * flags::	Any flags needed for controlling the behaviour of the stack.
 */
void init_dom_stack(struct dom_stack *stack, enum dom_stack_flag flags);
/** Release a DOM stack
 *
 * Free all resources collected by the stack.
 *
 * stack::	The stack to release. */
void done_dom_stack(struct dom_stack *stack);

/** Add a context to the stack
 *
 * This is needed if either you want to have the stack allocated objects for
 * created states and/or if you want to install callbacks for pushing or
 * popping.
 *
 * stack::	  The stack where the context should be created.
 * data::	  Private data to be stored in ref:[dom_stack_context.data].
 * context_info:: Information about state objects and node callbacks.
 *
 * Returns a pointer to the newly created context or NULL. */
struct dom_stack_context *
add_dom_stack_context(struct dom_stack *stack, void *data,
		      struct dom_stack_context_info *context_info);

/** Unregister a stack context
 * This should be done especially for temporary stack contexts (without any
 * callbacks) so that they do not increasing the memory usage. */
void done_dom_stack_context(struct dom_stack *stack, struct dom_stack_context *context);

/** Push a node onto the stack
 *
 * Makes the pushed node the new top of the stack.
 *
 * stack::	The stack to push onto.
 * node::	The node to push onto the stack.
 *
 * If an error occurs the node is released with ref:[done_dom_node] and NULL is
 * returned. Else the pushed node is returned. */
struct dom_node *push_dom_node(struct dom_stack *stack, struct dom_node *node);

/** Pop the top stack state
 *
 * stack::	The stack to pop from. */
void pop_dom_node(struct dom_stack *stack);

/** Conditionally pop the stack states
 *
 * Searches the stack (using ref:[search_dom_stack]) for a specific node and
 * pops all states until that particular state is met.
 *
 * NOTE: The popping is stopped if an immutable state is encountered. */
void pop_dom_nodes(struct dom_stack *stack, enum dom_node_type type,
		   struct dom_string *string);

/** Pop all states until target state
 *
 * Pop all stack states until a specific state is reached. The target state
 * is also popped.
 *
 * stack::	The stack to pop from.
 * target::	The state to pop until and including. */
void pop_dom_state(struct dom_stack *stack, struct dom_stack_state *target);

/** Search the stack states
 *
 * The string comparison is done against the ref:[dom_node.string] member of
 * the of the state nodes.
 *
 * stack::	The stack to search in.
 * type::	The type of node to match against.
 * string::	The string to match against.
 *
 * Returns a state that matched the type and string or NULL. */
struct dom_stack_state *
search_dom_stack(struct dom_stack *stack, enum dom_node_type type,
		 struct dom_string *string);

/** Walk all nodes reachable from a given node
 *
 * Visits each node in the DOM tree rooted at a given node, pre-order style.
 *
 * stack::	The stack to use for walking the nodes.
 * root::	The root node to start from.
 *
 * It is assummed that the given stack has been initialised with
 * ref:[init_dom_stack] and that the caller already added one or more
 * context to the stack. */
void walk_dom_nodes(struct dom_stack *stack, struct dom_node *root);

#endif
