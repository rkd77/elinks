/** The DOM stack interface.
 *
 * @file dom/stack.h
 *
 * The DOM stack interface is used by the parser when building a DOM
 * tree and by the various DOM tree traversers. It allows for several
 * stack contexts to be registered, each defining their own type
 * specific node handlers or callbacks (#dom_stack_callback_T) which
 * will be called upon a node being pushed onto or popped from the
 * stack. As an example a example, by defining DOM_STACK_TRACE it is be
 * possible to add a DOM stack tracer (using #add_dom_stack_tracer) to
 * debug all push and pop operations.
 *
 * The stack interface provides a method for automatically having
 * objects allocated when a new node is pushed onto the stack. This
 * object can then be used for storing private state information
 * belonging to the individual contexts. This is done by setting the
 * #dom_stack_context_info.object_size member to a non-zero value,
 * usually using sizeof().
 *
 * States on the stack can be marked immutable meaning that they will
 * be "locked" down so that any operations to pop them will fail. This
 * can be used when parsing a "subdocument", e.g. output from
 * ECMAScripts "document.write" function, where badly formatted SGML
 * should not be allowed to change the root document.
 *
 * In some situations, it may be desired to avoid building a complete
 * DOM tree in memory when only one document traversal is required, for
 * example when highlighting SGML code. This can be done by passing the
 * #DOM_STACK_FLAG_FREE_NODES flag to #init_dom_stack. Nodes that are
 * popped from the stack will immediately be deallocated. A pop node
 * callback can also request that a node is deallocated and removed from
 * the DOM tree by returning the #DOM_CODE_FREE_NODE return code. An
 * example of this can be seen in the DOM configuration module where
 * comment nodes may be pruned from the tree.
 */
/* TODO: Write about:
 * - How to access stack states and context state objects.
 * - Using search_dom_stack() and walk_dom_nodes().
 */

#ifndef EL_DOM_STACK_H
#define EL_DOM_STACK_H

#include "dom/code.h"
#include "dom/node.h"
#include "util/error.h"
#include "util/hash.h"

struct dom_stack;

/** DOM stack callback
 *
 * Used by contexts, for 'hooking' into the node traversing.
 *
 * This callback must not call done_dom_node() to free the node it
 * gets as a parameter, because call_dom_node_callbacks() may be
 * intending to call more callbacks for the same node.  Instead, the
 * callback can return ::DOM_CODE_FREE_NODE, and the node will then
 * be freed after the callbacks of all contexts have been called.  */
typedef enum dom_code
	(*dom_stack_callback_T)(struct dom_stack *, struct dom_node *, void *);

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
	/** Whether this stack state can be popped with pop_dom_node(),
	 * pop_dom_nodes(), or pop_dom_state(). */
	unsigned int immutable:1;
};

/** DOM stack context info
 *
 * To add a DOM stack context define this struct either statically or on the
 * stack. */
struct dom_stack_context_info {
	/**
	 * The number of bytes to allocate on the stack for each state's
	 * data member.  Zero means no state data should be allocated.
	 * */
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
	 * struct #sgml_node_info). E.g.  whether an element node is optional.
	 */
	unsigned char *state_objects;

	/** Info about node callbacks and such. */
	struct dom_stack_context_info *info;
};

/** Flags for controlling the DOM stack */
enum dom_stack_flag {
	/** No flag needed. */
	DOM_STACK_FLAG_NONE = 0,

	/** Free nodes when popping by calling done_dom_node(). */
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
	 * #dom_stack_callback_T functions. */
	struct dom_stack_context *current;
};

/** Check whether stack is empty or not
 *
 * @param stack		The stack to check.
 *
 * @returns		Non-zero if stack is empty. */
#define dom_stack_is_empty(stack) \
	(!(stack)->states || (stack)->depth == 0)

/** Access state by offset from top
 *
 * @param stack		The stack to fetch the state from.
 * @param top_offset	The offset from the stack top, zero is the top.
 *
 * @returns		The requested state. */
static inline struct dom_stack_state *
get_dom_stack_state(struct dom_stack *stack, int top_offset)
{
	assertm(stack->depth - 1 - top_offset >= 0,
		"Attempting to access invalid state");
	return &stack->states[stack->depth - 1 - top_offset];
}

/** Access the stack top
 *
 * @param stack		The stack to get the top state from.
 *
 * @returns		The top state. */
#define get_dom_stack_top(stack) \
	get_dom_stack_state(stack, 0)

/** Access context specific state data
 *
 * Similar to ref:[get_dom_stack_state], this will fetch the data
 * associated with the state for the given context.
 *
 * @param context	The context to get data from.
 * @param state		The stack state to get data from.
 *
 * @returns		The state data or NULL if
 *			#dom_stack_context_info.object_size was zero. */
static inline void *
get_dom_stack_state_data(struct dom_stack_context *context,
			 struct dom_stack_state *state)
{
	size_t object_size = context->info->object_size;

	if (!object_size) return NULL;

	assert(context->state_objects);

	return (void *) &context->state_objects[state->depth * object_size];
}

/*#define DOM_STACK_TRACE*/

/** Get debug info from the DOM stack
 *
 * Define `DOM_STACK_TRACE` to have debug info about the nodes added printed to
 * the log. It will define add_dom_stack_tracer() to not be a no-op.
 *
 * Run as:
 *
 *	ELINKS_LOG=/tmp/dom-dump.txt ./elinks -no-connect URL
 *
 * to have the debug dumped into a file. */
#ifdef DOM_STACK_TRACE
#define add_dom_stack_tracer(stack, name) \
	add_dom_stack_context(stack, name, &dom_stack_trace_context_info)
extern struct dom_stack_context_info dom_stack_trace_context_info;
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
 *
 * @param stack		Pointer to a (preallocated) stack.
 * @param flags		Any flags needed for controlling the behaviour of the stack.
 */
void init_dom_stack(struct dom_stack *stack, enum dom_stack_flag flags);

/** Release a DOM stack
 *
 * Free all resources collected by the stack.
 *
 * @param stack		The stack to release. */
void done_dom_stack(struct dom_stack *stack);

/** Add a context to the stack
 *
 * This is needed if either you want to have the stack allocated objects for
 * created states and/or if you want to install callbacks for pushing or
 * popping.
 *
 * @param stack		The stack where the context should be created.
 * @param data		Private data to be stored in ref:[dom_stack_context.data].
 * @param context_info	Information about state objects and node callbacks.
 *
 * @returns		A pointer to the newly created context or NULL. */
struct dom_stack_context *
add_dom_stack_context(struct dom_stack *stack, void *data,
		      struct dom_stack_context_info *context_info);

/** Unregister a stack context
 *
 * This should be done especially for temporary stack contexts (without any
 * callbacks) so that they do not increasing the memory usage. */
void done_dom_stack_context(struct dom_stack *stack, struct dom_stack_context *context);

/** Push a node onto the stack
 *
 * Makes the pushed node the new top of the stack.
 *
 * @param stack		The stack to push onto.
 * @param node		The node to push onto the stack.
 *
 * @returns		If an error occurs the node is released with
 *			#done_dom_node and NULL is returned. Else the pushed
 *			node is returned. */
enum dom_code push_dom_node(struct dom_stack *stack, struct dom_node *node);

/** Pop the top stack state
 *
 * @param stack		The stack to pop from. */
void pop_dom_node(struct dom_stack *stack);

/** Conditionally pop the stack states
 *
 * Searches the stack (using ref:[search_dom_stack]) for a specific node and
 * pops all states until that particular state is met.
 *
 * @note		The popping is stopped if an immutable state is
 *			encountered. */
void pop_dom_nodes(struct dom_stack *stack, enum dom_node_type type,
		   struct dom_string *string);

/** Pop all states until target state
 *
 * Pop all stack states until a specific state is reached. The target state
 * is also popped.
 *
 * @param stack		The stack to pop from.
 * @param target	The state to pop until and including. */
void pop_dom_state(struct dom_stack *stack, struct dom_stack_state *target);

/** Search the stack states
 *
 * The string comparison is done against the ref:[dom_node.string] member of
 * the of the state nodes.
 *
 * @param stack		The stack to search in.
 * @param type		The type of node to match against.
 * @param string	The string to match against.
 *
 * @returns		A state that matched the type and string or NULL. */
struct dom_stack_state *
search_dom_stack(struct dom_stack *stack, enum dom_node_type type,
		 struct dom_string *string);

/** Walk all nodes reachable from a given node
 *
 * Visits each node in the DOM tree rooted at a given node, pre-order style.
 *
 * @param stack		The stack to use for walking the nodes.
 * @param root		The root node to start from.
 *
 * It is assummed that the given stack has been initialised with
 * #init_dom_stack and that the caller already added one or more context
 * to the stack. */
void walk_dom_nodes(struct dom_stack *stack, struct dom_node *root);

#endif
