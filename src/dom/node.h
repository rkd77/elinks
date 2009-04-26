/** DOM node module
 *
 * @file dom/node.h
 *
 * This module defines the various node and node list data structures
 * and functionality to modify and access them, such as adding a node as
 * a child to a given node and getting the text string of a node as
 * defined by the DOM specification.
 *
 * @par Node hierarchy
 *
 * DOM documents are represented as a collection of nodes arranged in a
 * hierarchic structure. At the root is either a #DOM_NODE_DOCUMENT or
 * #DOM_NODE_DOCUMENT_FRAGMENT node, each of which may have multiple
 * child nodes. There is a well-defined order that dictates which child
 * nodes may be descendants of a given type of node. For example, text
 * and attribute nodes can have no children, while elements node may
 * have both attribute and element nodes as children but with each type
 * in different node lists. The hierarchy is somewhat encoded in the
 * type specific node data, however, certain node types also define
 * "custom" node lists for conveniently storing additional "embedded"
 * data, such as processing instruction nodes having an attribute node
 * list for conveniently accessing variable-value pairs given for
 * XML-specific processing instructions:
 *
 *	@verbatim <?xml version="1.0"?> @endverbatim
 *
 * @par Node lists
 *
 * There are two types of list: unordered (the default) and
 * alphabetically ordered (also called "maps"). Both types of list
 * stores all contained nodes in the index-oriented #dom_node_list data
 * structure.
 *
 * When inserting a node into a list, first use either
 * #get_dom_node_list_index or #get_dom_node_map_index (depending on
 * whether the list is unordered or ordered respectively) to calculate
 * the index at which to insert the new node. Then use
 * #add_to_dom_node_list to insert the node in the list at the given
 * position. Alternatively (and mostly preferred), simply use
 * #add_dom_node to have all of the above done automatically plus some
 * additional checks.
 *
 * A variety of node list accessors are defined. The node structure does
 * not define any "next" or "previous" members to get siblings due to
 * reduce memory usage (this might have to change --jonas). Instead, use
 * #get_dom_node_next and #get_dom_node_next to access siblings. To
 * lookup the existence of a node in a sorted node list (map) use
 * #get_dom_node_map_entry. If a specific and unique node subtype should
 * be found use #get_dom_node_child that given a parent node will find a
 * child node based on a specific child node type and subtype. Finally,
 * list can be iterated in forward and reverse order using
 * #foreach_dom_node and #foreachback_dom_node.
 */

#ifndef EL_DOM_NODE_H
#define EL_DOM_NODE_H

#include "dom/string.h"

struct dom_node_list;
struct dom_document;

/** DOM node types */
enum dom_node_type {
	DOM_NODE_UNKNOWN		=  0, /**< Node type used internally. */

	DOM_NODE_ELEMENT		=  1, /**< Element node */
	DOM_NODE_ATTRIBUTE		=  2, /**< Attribute node */
	DOM_NODE_TEXT			=  3, /**< Text node */
	DOM_NODE_CDATA_SECTION		=  4, /**< CData section node */
	DOM_NODE_ENTITY_REFERENCE	=  5, /**< Entity reference node */
	DOM_NODE_ENTITY			=  6, /**< Entity node */
	DOM_NODE_PROCESSING_INSTRUCTION	=  7, /**< Processing instruction node */
	DOM_NODE_COMMENT		=  8, /**< Comment node */
	DOM_NODE_DOCUMENT		=  9, /**< Document root node */
	DOM_NODE_DOCUMENT_TYPE		= 10, /**< Document type (DTD) node */
	DOM_NODE_DOCUMENT_FRAGMENT	= 11, /**< Document fragment node */
	DOM_NODE_NOTATION		= 12, /**< Notation node */

	DOM_NODES			      /**< The number of DOM nodes */
};

/* Following is the node specific data structures. They may contain no
 * more than 4 pointers or something equivalent. */

/* The document URI is stored in the string / length members. */
struct dom_document_node {
	/* The document. */
	struct dom_document *document;

	/* The child nodes. May be NULL. Ordered like they where inserted. */
	/* FIXME: Should be just one element (root) node reference. */
	struct dom_node_list *children;
};

struct dom_id {
	struct dom_string public_id;
	struct dom_string system_id;
};

struct dom_doctype_subset_info {
	struct dom_string internal;
	struct dom_id external;
};

struct dom_document_type_node {
	/* These are really maps and should be sorted alphabetically. */
	struct dom_node_list *entities;
	struct dom_node_list *notations;

	/* The string/length members of dom_node hold the name of the document
	 * type "<!DOCTYPE {name} ...>". This holds the ids for the external
	 * subset and the string of the internal subset. */
	struct dom_doctype_subset_infot *subset;
};

/* Element nodes are indexed nodes stored in node lists of either
 * other child nodes or the root node. */
struct dom_element_node {
	/* The child nodes. May be NULL. Ordered like they where inserted. */
	struct dom_node_list *children;

	/* Only element nodes can have attributes and element nodes can only be
	 * child nodes so the map is put here.
	 *
	 * The @map may be NULL if there are none. The @map nodes are sorted
	 * alphabetically according to the attributes name so it has fast
	 * lookup. */
	struct dom_node_list *map;

	/* For <xsl:stylesheet ...> elements this holds the offset of
	 * 'stylesheet' */
	uint16_t namespace_offset;

	/* Special implementation dependent type specifier for example
	 * containing an enum value representing the element to reduce string
	 * comparing and only do one fast find mapping. */
	uint16_t type;
};

/* Attribute nodes are named nodes stored in a node map of an element node. */
struct dom_attribute_node {
	/* The string that hold the attribute value. The @string / @length
	 * members of {struct dom_node} holds the name that identifies the node
	 * in the map. */
	struct dom_string value;

	/* For xml:lang="en" attributes this holds the offset of 'lang' */
	uint16_t namespace_offset;

	/* Special implementation dependent type specifier. For HTML it (will)
	 * contain an enum value representing the attribute HTML_CLASS, HTML_ID etc.
	 * to reduce string comparing and only do one fast find mapping. */
	uint16_t type;

	/* The attribute value is delimited by quotes. Can be NUL, ' or ". */
	unsigned char quoted;

	/* Was the attribute specified in the DTD as a default attribute or was
	 * it added from the document source. */
	unsigned int specified:1;

	/* Has the node->string been converted to internal charset. */
	unsigned int converted:1;

	/* Is the attribute a unique identifier. */
	unsigned int id:1;

	/* The attribute value references some other resource */
	unsigned int reference:1;
};

struct dom_text_node {
	/* The number of newlines the text string contains */
	unsigned int newlines;

	/* We will need to add text nodes even if they contain only whitespace.
	 * In order to quickly identify such nodes this member is used. */
	unsigned int only_space:1;

	/* Has the node->string been converted to internal charset. */
	unsigned int converted:1;
};

enum dom_proc_instruction_type {
	DOM_PROC_INSTRUCTION,

	/* Keep this group sorted */
	DOM_PROC_INSTRUCTION_XML,		/* XML header */
	DOM_PROC_INSTRUCTION_XML_STYLESHEET,	/* XML stylesheet link */

	DOM_PROC_INSTRUCTION_TYPES
};

struct dom_proc_instruction_node {
	/* The target of the processing instruction (xml for '<?xml ...  ?>')
	 * is in the @string / @length members. */
	/* This holds the value to be processed */
	struct dom_string instruction;

	/* For fast checking of the target type */
	uint16_t type; /* enum dom_proc_instruction_type */

	/* For some processing instructions like xml the instructions contain
	 * attributes and those attribute can be collected in this @map. */
	struct dom_node_list *map;
};

union dom_node_data {
	struct dom_document_node	 document;
	struct dom_document_type_node	 document_type;
	struct dom_element_node		 element;
	struct dom_attribute_node	 attribute;
	struct dom_text_node		 text;
	struct dom_id			 notation;
	/* For entities string/length hold the notation name */
	struct dom_id			 entity;
	struct dom_proc_instruction_node proc_instruction;

	/* Node types without a union member yet (mostly because it hasn't
	 * been necessary):
	 *
	 * DOM_NODE_CDATA_SECTION:	Use dom_text_node?
	 * DOM_NODE_DOCUMENT_FRAGMENT:	struct dom_node_list children;
	 * DOM_NODE_ENTITY_REFERENCE:	unicode_val_T
	 * DOM_NODE_COMMENT
	 */
};

/** DOM node
 *
 * The node data structure is an abstract container that can be used to
 * represent the hierarchic structure of a document, such as relation
 * between elements, attributes, etc.
 *
 * @note	This structure is size critical so keep ordering to make
 *		it easier to pack and avoid unneeded members.
 */
struct dom_node {
	/** The type of the node. Holds a #dom_node_type enum value. */
	uint16_t type; /* -> enum dom_node_type */

	/** Was the node string allocated? */
	unsigned int allocated:1;

	/** Type specific node string. Can contain either stuff like
	 * element name or for attributes the attribute name. */
	struct dom_string string;

	/** The parent node. The parent node is NULL for the root node. */
	struct dom_node *parent;

	/** Type specific node data. */
	union dom_node_data data;
};

/** DOM node list
 *
 * A node list can be used for storing indexed nodes. If a node list
 * should be sorted alphabetically use the #get_dom_node_map_index
 * function to find the index of new nodes before inserting them. */
struct dom_node_list {
	size_t size;
	struct dom_node *entries[1];
};

#define foreach_dom_node(list, node, i)			\
	for ((i) = 0; (i) < (list)->size; (i)++)	\
		if (((node) = (list)->entries[(i)]))

#define foreachback_dom_node(list, node, i)		\
	for ((i) = (list)->size - 1; (i) > 0; (i)--)	\
		if (((node) = (list)->entries[(i)]))

#define is_dom_node_list_member(list, member)		\
	((list) && 0 <= (member) && (member) < (list)->size)

/* Adds @node to the list pointed to by @list_ptr at the given @position. If
 * @position is -1 the node is added at the end. */
struct dom_node_list *
add_to_dom_node_list(struct dom_node_list **list_ptr,
		     struct dom_node *node, int position);

void done_dom_node_list(struct dom_node_list *list);

/* Returns the position or index where the @node has been inserted into the
 * 'default' list of the @parent node. (Default means use get_dom_node_list()
 * to acquire the list to search in. Returns -1, if the node is not found. */
int get_dom_node_list_index(struct dom_node *parent, struct dom_node *node);

/* Returns the position or index where the @node should be inserted into the
 * node @list in order to the list to be alphabetically sorted.  Assumes that
 * @list is already sorted properly. */
int get_dom_node_map_index(struct dom_node_list *list, struct dom_node *node);

/* Returns the previous sibling to the node. */
struct dom_node *get_dom_node_prev(struct dom_node *node);

/* Returns the next sibling to the node. */
struct dom_node *get_dom_node_next(struct dom_node *node);

/* Returns first text node of the element or NULL. */
struct dom_node *
get_dom_node_child(struct dom_node *node, enum dom_node_type child_type,
		   int16_t child_subtype);

/* Looks up the @node_map for a node matching the requested type and name.
 * The @subtype maybe be 0 indication unknown subtype and only name should be
 * tested else it will indicate either the element or attribute private
 * subtype. */
struct dom_node *
get_dom_node_map_entry(struct dom_node_list *node_map,
		       enum dom_node_type type, uint16_t subtype,
		       struct dom_string *name);

/* Removes the node and all its children and free()s itself.
 * A dom_stack_callback_T must not use this to free the node
 * it gets as a parameter.  */
void done_dom_node(struct dom_node *node);

#ifndef DEBUG_MEMLEAK

/* The allocated argument is used as the value of node->allocated if >= 0.
 * Use -1 to default node->allocated to the value of parent->allocated. */

struct dom_node *
init_dom_node_at(struct dom_node *parent, enum dom_node_type type,
		 struct dom_string *string, int allocated);

#define init_dom_node(type, string, allocated) \
	init_dom_node_at(NULL, type, string, allocated)

#define add_dom_node(parent, type, string) \
	init_dom_node_at(parent, type, string, -1)

#else
struct dom_node *
init_dom_node_at(unsigned char *file, int line,
		 struct dom_node *parent, enum dom_node_type type,
		 struct dom_string *string, int allocated);

#define init_dom_node(type, string, allocated) \
	init_dom_node_at(__FILE__, __LINE__, NULL, type, string, allocated)

#define add_dom_node(parent, type, string) \
	init_dom_node_at(__FILE__, __LINE__, parent, type, string, -1)

#endif /* DEBUG_MEMLEAK */

#define add_dom_element(parent, string) \
	add_dom_node(parent, DOM_NODE_ELEMENT, string)

static inline struct dom_node *
add_dom_attribute(struct dom_node *parent, struct dom_string *name,
		  struct dom_string *value)
{
	struct dom_node *node = add_dom_node(parent, DOM_NODE_ATTRIBUTE, name);

	if (node && value) {
		struct dom_string *str = &node->data.attribute.value;

		if (node->allocated) {
			if (!init_dom_string(str, value->string, value->length)) {
				done_dom_node(node);
				return NULL;
			}
		} else {
			copy_dom_string(str, value);
		}
	}

	return node;
}

static inline struct dom_node *
add_dom_proc_instruction(struct dom_node *parent, struct dom_string *string,
			 struct dom_string *instruction)
{
	struct dom_node *node = add_dom_node(parent, DOM_NODE_PROCESSING_INSTRUCTION, string);

	if (node && instruction) {
		struct dom_string *str = &node->data.proc_instruction.instruction;

		if (node->allocated) {
			if (!init_dom_string(str, instruction->string, instruction->length)) {
				done_dom_node(node);
				return NULL;
			}
		} else {
			copy_dom_string(str, instruction);
		}
	}

	return node;
}

/* Compare two nodes returning non-zero if they differ. */
int dom_node_casecmp(struct dom_node *node1, struct dom_node *node2);

/* Returns the name of the node in an allocated string. */
struct dom_string *get_dom_node_name(struct dom_node *node);

/* Returns the value of the node or NULL if no value is defined for the node
 * type. */
struct dom_string *get_dom_node_value(struct dom_node *node);

/* Returns the name used for identifying the node type. */
struct dom_string *get_dom_node_type_name(enum dom_node_type type);

/** Based on the type of the @a parent and the node @a type return a
 * proper list or NULL. This is useful when adding a node to a parent
 * node.
 *
 * With a <code>struct dom_node_list **list</code> returned by this
 * function, there are four possibilities:
 *
 * - <code>list == NULL</code>.  This means @a parent does not support
 *   child nodes of the given @a type.
 *
 * - <code>*list == NULL</code>.  This means @a parent does not yet
 *   have any child nodes of the given @a type and so no list has been
 *   allocated for them.  Callers should treat the lack of a list in
 *   the same way as an empty list.
 *
 * - <code>(*list)->size == 0</code>.  This is an empty list.  It is
 *   unspecified whether the DOM code keeps such lists; it could
 *   instead change them back to NULL.
 *
 * - <code>(*list)->size != 0</code>.  This is a nonempty list.
 *   However, the nodes in it might not actually be of the given
 *   @a type because some lists are used for multiple types.  */
static inline struct dom_node_list **
get_dom_node_list_by_type(struct dom_node *parent, enum dom_node_type type)
{
	switch (parent->type) {
	case DOM_NODE_DOCUMENT:
		return &parent->data.document.children;

	case DOM_NODE_ELEMENT:
		switch (type) {
		case DOM_NODE_ATTRIBUTE:
			return &parent->data.element.map;

		default:
			return &parent->data.element.children;
		}

	case DOM_NODE_DOCUMENT_TYPE:
		switch (type) {
		case DOM_NODE_ENTITY:
			return &parent->data.document_type.entities;

		case DOM_NODE_NOTATION:
			return &parent->data.document_type.notations;

		default:
			return NULL;
		}

	case DOM_NODE_PROCESSING_INSTRUCTION:
		switch (type) {
		case DOM_NODE_ATTRIBUTE:
			return &parent->data.proc_instruction.map;

		default:
			return NULL;
		}

	default:
		return NULL;
	}
}

#define get_dom_node_list(parent, node) \
	get_dom_node_list_by_type(parent, (node)->type)

#endif
