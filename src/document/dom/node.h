/* $Id: node.h,v 1.7 2005/07/12 15:46:34 jonas Exp $ */

#ifndef EL__DOCUMENT_DOM_NODE_H
#define EL__DOCUMENT_DOM_NODE_H

#include "util/hash.h"

struct dom_node_list;

enum dom_node_type {
	DOM_NODE_ELEMENT		=  1,
	DOM_NODE_ATTRIBUTE		=  2,
	DOM_NODE_TEXT			=  3,
	DOM_NODE_CDATA_SECTION		=  4,
	DOM_NODE_ENTITY_REFERENCE	=  5,
	DOM_NODE_ENTITY			=  6,
	DOM_NODE_PROCESSING_INSTRUCTION	=  7,
	DOM_NODE_COMMENT		=  8,
	DOM_NODE_DOCUMENT		=  9,
	DOM_NODE_DOCUMENT_TYPE		= 10,
	DOM_NODE_DOCUMENT_FRAGMENT	= 11,
	DOM_NODE_NOTATION		= 12,

	DOM_NODES
};

/* Following is the node specific datastructures. They may contain no more
 * than 3 pointers or something equivalent. */

struct dom_node_id_item {
	/* The attibute node containing the id value */
	struct dom_node *id_attribute;

	/* The path to the node. This can be passed to the DOM navigator to
	 * locate the node. The path should not contain the id node itself.
	 * E.g. for <a id="..."> element "html/body/p/span". */
	unsigned char *path;

	/* The node with the @id attribute */
	struct dom_node *node;
};

struct dom_document_node {
	/* The document URI is stored in the string / length members. */
	/* An id to node hash for fast lookup. */
	struct hash *element_ids; /* -> {struct dom_node_id_item} */

	/* Any meta data the root node carries such as document type nodes,
	 * entity and notation map nodes and maybe some internal CSS stylesheet
	 * node. */
	struct dom_node_list *meta_nodes;

	/* The child nodes. May be NULL. Ordered like they where inserted. */
	struct dom_node_list *children;
};

struct dom_id {
	unsigned char *public_id;
	uint16_t pid_length;
	uint16_t sid_length;
	unsigned char *system_id;
};

struct dom_doctype_subset_info {
	uint16_t internallen;
	unsigned char *internal;
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
	unsigned char *value;
	uint16_t valuelen;

	/* For xml:lang="en" attributes this holds the offset of 'lang' */
	uint16_t namespace_offset;

	/* Special implementation dependent type specifier. For HTML it (will)
	 * contain an enum value representing the attribute HTML_CLASS, HTML_ID etc.
	 * to reduce string comparing and only do one fast find mapping. */
	uint16_t type;

	/* Was the attribute specified in the DTD as a default attribute or was
	 * it added from the document source. */
	unsigned int specified:1;

	/* Was the node->string allocated */
	unsigned int allocated:1;

	/* Has the node->string been converted to internal charset. */
	unsigned int converted:1;

	/* Is the attribute a unique identifier meaning the owner (element)
	 * should be added to the document nodes @element_id hash. */
	unsigned int id:1;

	/* The attribute value references some other resource */
	unsigned int reference:1;

	/* The attribute value is delimited by quotes */
	unsigned int quoted:1;
};

struct dom_text_node {
	/* The number of newlines the text string contains */
	unsigned int newlines;

	/* We will need to add text nodes even if they contain only whitespace.
	 * In order to quickly identify such nodes this member is used. */
	unsigned int only_space:1;

	/* Was the node->string allocated */
	unsigned int allocated:1;

	/* Has the node->string been converted to internal charset. */
	unsigned int converted:1;
};

enum dom_proc_instruction_type {
	DOM_PROC_INSTRUCTION,

	/* Keep this group sorted */
	DOM_PROC_INSTRUCTION_DBHTML,	/* DocBook toolchain instruction */
	DOM_PROC_INSTRUCTION_ELINKS,	/* Internal instruction hook */
	DOM_PROC_INSTRUCTION_XML,	/* XML instructions */

	DOM_PROC_INSTRUCTION_TYPES
};

struct dom_proc_instruction_node {
	/* The target of the processing instruction (xml for '<?xml ...  ?>')
	 * is in the @string / @length members. */
	/* This holds the value to be processed */
	unsigned char *instruction;
	uint16_t instructionlen;

	/* For fast checking of the target type */
	uint16_t type; /* enum dom_proc_instruction_type */

	/* For some processing instructions like xml the instructions contain
	 * attributes and those attribute can be collected in this @map. */
	struct dom_node_list *map;
};

/* This structure is size critical so keep ordering to make it easier to pack
 * and avoid unneeded members. */
struct dom_node {
	/* The type of the node */
	uint16_t type; /* -> enum dom_node_type */

	/* Can contain either stuff like element name or for attributes the
	 * attribute name. */
	uint16_t length;
	unsigned char *string;

	/* Various info depending on the type of the node. */
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

		/* Node types without a union member yet
		 *
		 * DOM_NODE_CDATA_SECTION,
		 * DOM_NODE_COMMENT,
		 * DOM_NODE_DOCUMENT_FRAGMENT,
		 * DOM_NODE_ENTITY_REFERENCE,
		 */
	} data;
};

/* A node list can be used for storing indexed nodes */
struct dom_node_list {
	size_t size;
	struct dom_node *entries[1];
};

#define foreach_dom_node(iterator, node, list)		\
	for (i = 0; i < (list)->size; i++)		\
		if (((node) = (list)->entries[i]))

#define foreachback_dom_node(iterator, node, list)	\
	for (i = (list)->size - 1; i > 0; i--)		\
		if (((node) = (list)->entries[i]))

#define is_dom_node_list_member(list, member)		\
	((list) && 0 <= (member) && (member) < (list)->size)

/* Adds @node to the list pointed to by @list_ptr at the given @position. If
 * @position is -1 the node is added at the end. */
struct dom_node_list *
add_to_dom_node_list(struct dom_node_list **list_ptr,
		     struct dom_node *node, int position);

void done_dom_node_list(struct dom_node_list *list);

/* Returns the position or index where the @node should be inserted into the
 * node @list in order to the list to be alphabetically sorted.  Assumes that
 * @list is already sorted properly. */
int get_dom_node_map_index(struct dom_node_list *list, struct dom_node *node);

/* Looks up the @node_map for a node matching the requested type and name.
 * The @subtype maybe be 0 indication unknown subtype and only name should be
 * tested else it will indicate either the element or attribute private
 * subtype. */
struct dom_node *
get_dom_node_map_entry(struct dom_node_list *node_map,
		       enum dom_node_type type, uint16_t subtype,
		       unsigned char *name, int namelen);

struct dom_node *
init_dom_node_(unsigned char *file, int line,
		struct dom_node *parent, enum dom_node_type type,
		unsigned char *string, uint16_t length);
#define init_dom_node(type, string, length) init_dom_node_(__FILE__, __LINE__, NULL, type, string, length)
#define add_dom_node(parent, type, string, length) init_dom_node_(__FILE__, __LINE__, parent, type, string, length)

#define add_dom_element(parent, string, length) \
	add_dom_node(parent, DOM_NODE_ELEMENT, string, length)

static inline struct dom_node *
add_dom_attribute(struct dom_node *parent, unsigned char *string, int length,
		  unsigned char *value, uint16_t valuelen)
{
	struct dom_node *node = add_dom_node(parent, DOM_NODE_ATTRIBUTE, string, length);

	if (node && value) {
		node->data.attribute.value    = value;
		node->data.attribute.valuelen = valuelen;
	}

	return node;
}

static inline struct dom_node *
add_dom_proc_instruction(struct dom_node *parent, unsigned char *string, int length,
			 unsigned char *instruction, uint16_t instructionlen)
{
	struct dom_node *node = add_dom_node(parent, DOM_NODE_PROCESSING_INSTRUCTION, string, length);

	if (node && instruction) {
		node->data.proc_instruction.instruction    = instruction;
		node->data.proc_instruction.instructionlen = instructionlen;
	}

	return node;
}

/* Removes the node and all its children and free()s itself */
void done_dom_node(struct dom_node *node);

/* Returns the name of the node in an allocated string. */
unsigned char *get_dom_node_name(struct dom_node *node);

/* Returns the value of the node in an allocated string.
 * @codepage denotes how entity strings should be decoded. */
unsigned char *get_dom_node_value(struct dom_node *node, int codepage);

/* Returns the name used for identifying the node type. */
unsigned char *get_dom_node_type_name(enum dom_node_type type);

/* Returns a pointer to a node list containing attributes. */
#define get_dom_node_attributes(node)					\
	((node)->type == DOM_NODE_ELEMENT ? &(node)->data.element.map	\
					  : NULL)

static inline struct dom_node_list **
get_dom_node_list(struct dom_node *parent, struct dom_node *node)
{
	switch (parent->type) {
	case DOM_NODE_DOCUMENT:
		return &parent->data.document.children;

	case DOM_NODE_ELEMENT:
		switch (node->type) {
		case DOM_NODE_ATTRIBUTE:
			return &parent->data.element.map;

		default:
			return &parent->data.element.children;
		}

	case DOM_NODE_DOCUMENT_TYPE:
		switch (node->type) {
		case DOM_NODE_ENTITY:
			return &parent->data.document_type.entities;

		case DOM_NODE_NOTATION:
			return &parent->data.document_type.notations;

		default:
			return NULL;
		}

	case DOM_NODE_PROCESSING_INSTRUCTION:
		switch (node->type) {
		case DOM_NODE_ATTRIBUTE:
			return &parent->data.proc_instruction.map;

		default:
			return NULL;
		}

	default:
		return NULL;
	}
}

#endif
