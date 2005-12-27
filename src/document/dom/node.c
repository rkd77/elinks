/* The DOM node handling */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/dom/node.h"
#include "intl/charsets.h"
#include "util/hash.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"


static void done_dom_node_data(struct dom_node *node);

/* Node lists */

#define DOM_NODE_LIST_GRANULARITY 0x7

#define DOM_NODE_LIST_BLOCK_SIZE \
	(ALIGN_MEMORY_SIZE(1, DOM_NODE_LIST_GRANULARITY) * sizeof(struct dom_node *))

/* The node list struct has one node pointer */
#define DOM_NODE_LIST_SIZE(size) \
	((size - 1) * sizeof(struct dom_node *) + sizeof(struct dom_node_list))

static inline struct dom_node_list *
realloc_dom_node_list(struct dom_node_list **oldlist)
{
	struct dom_node_list *list = *oldlist;
	size_t size = list ? list->size : 0;
	size_t oldsize = ALIGN_MEMORY_SIZE(size, DOM_NODE_LIST_GRANULARITY);
	size_t newsize = ALIGN_MEMORY_SIZE(size + 1, DOM_NODE_LIST_GRANULARITY);

	if (newsize <= oldsize) return list;

	list = mem_realloc(list, DOM_NODE_LIST_SIZE(newsize));
	if (!list) return NULL;

	/* If this is the first reallocation clear the size */
	if (!size) list->size = 0;

	/* Clear the new block of entries */
	memset(&list->entries[oldsize], 0, DOM_NODE_LIST_BLOCK_SIZE);

	*oldlist = list;

	return list;
}

struct dom_node_list *
add_to_dom_node_list(struct dom_node_list **list_ptr,
		     struct dom_node *node, int position)
{
	struct dom_node_list *list;

	assert(list_ptr && node);

	list = realloc_dom_node_list(list_ptr);
	if (!list) return NULL;

	assertm(position < 0 || position <= list->size,
		"position out of bound %d > %zu", position, list->size);

	if (position < 0) {
		position = list->size;

	} else if (position < list->size) {
		/* Make room if we have to add the node in the middle of the list */
		struct dom_node **offset = &list->entries[position];
		size_t size = (list->size - position) * sizeof(*offset);

		memmove(offset + 1, offset, size);
	}

	list->size++;
	list->entries[position] = node;

	return list;
}

static void
del_from_dom_node_list(struct dom_node_list *list, struct dom_node *node)
{
	struct dom_node *entry;
	size_t i;

	if (!list) return;

	foreach_dom_node (list, entry, i) {
		size_t successors;

		if (entry != node) continue;

		successors = list->size - (i + 1);
		if (successors)
			memmove(&list->entries[i], &list->entries[i+1],
				sizeof(*list->entries) * successors);
		list->size--;
	}
}

void
done_dom_node_list(struct dom_node_list *list)
{
	struct dom_node *node;
	int i;

	assert(list);

	foreach_dom_node (list, node, i) {
		/* Avoid that the node start messing with the node list. */
		done_dom_node_data(node);
	}

	mem_free(list);
}


/* Node map */

struct dom_node_search {
	struct dom_node *key;
	unsigned int from, pos, to;
};

#define INIT_DOM_NODE_SEARCH(key, list) \
	{ (key), -1, 0, (list)->size, }

int
dom_node_casecmp(struct dom_node *node1, struct dom_node *node2)
{
	if (node1->type == node2->type) {
		switch (node1->type) {
		case DOM_NODE_ELEMENT:
			if (node1->data.element.type && node2->data.element.type)
				return node1->data.element.type - node2->data.element.type;
			break;

		case DOM_NODE_ATTRIBUTE:
			if (node1->data.attribute.type && node2->data.attribute.type)
				return node1->data.attribute.type - node2->data.attribute.type;
			break;

		default:
			break;
		}
	}

	return dom_string_casecmp(&node1->string, &node2->string);
}

static inline int
get_bsearch_position(struct dom_node_list *list, int from, int to)
{
	int pos = from + ((to - from) / 2);

	assertm(0 <= pos && pos < list->size, "pos %d", pos);
	return pos;
}

#define has_bsearch_node(from, to)	((from) + 1 < (to))

static inline struct dom_node *
dom_node_list_bsearch(struct dom_node_search *search, struct dom_node_list *list)
{
	assert(has_bsearch_node(search->from, search->to));

	do {
		int pos = get_bsearch_position(list, search->from, search->to);
		struct dom_node *node = list->entries[pos];
		int difference = dom_node_casecmp(search->key, node);

		search->pos = pos;

		if (!difference) return node;

		if (difference < 0) {
			search->to   = search->pos;
		} else {
			search->from = search->pos;
		}

	} while (has_bsearch_node(search->from, search->to));

	return NULL;
}

int get_dom_node_map_index(struct dom_node_list *list, struct dom_node *node)
{
	struct dom_node_search search = INIT_DOM_NODE_SEARCH(node, list);
	struct dom_node *match = dom_node_list_bsearch(&search, list);

	return match ? search.pos : search.to;
}

struct dom_node *
get_dom_node_map_entry(struct dom_node_list *list, enum dom_node_type type,
		       uint16_t subtype, struct dom_string *name)
{
	struct dom_node node = { type, INIT_DOM_STRING(name->string, name->length) };
	struct dom_node_search search = INIT_DOM_NODE_SEARCH(&node, list);

	if (subtype) {
		/* Set the subtype */
		switch (type) {
		case DOM_NODE_ELEMENT:
			node.data.element.type = subtype;
			break;
		case DOM_NODE_ATTRIBUTE:
			node.data.attribute.type = subtype;
			break;
		case DOM_NODE_PROCESSING_INSTRUCTION:
			node.data.proc_instruction.type = subtype;
			break;
		default:
			break;
		}
	}

	return dom_node_list_bsearch(&search, list);
}

int
get_dom_node_list_index(struct dom_node *parent, struct dom_node *node)
{
	struct dom_node_list **list = get_dom_node_list(parent, node);
	struct dom_node *entry;
	int i;

	if (!list) return -1;

	foreach_dom_node (*list, entry, i) {
		if (entry == node)
			return i;
	}

	return -1;
}

/* Nodes */

struct dom_node *
init_dom_node_(unsigned char *file, int line,
		struct dom_node *parent, enum dom_node_type type,
		unsigned char *string, size_t length)
{
#ifdef DEBUG_MEMLEAK
	struct dom_node *node = debug_mem_calloc(file, line, 1, sizeof(*node));
#else
	struct dom_node *node = mem_calloc(1, sizeof(*node));
#endif

	if (!node) return NULL;

	node->type   = type;
	node->parent = parent;
	set_dom_string(&node->string, string, length);

	if (parent) {
		struct dom_node_list **list = get_dom_node_list(parent, node);
		int sort = (type == DOM_NODE_ATTRIBUTE);
		int index;

		assertm(list, "Adding node %d to bad parent %d",
			node->type, parent->type);

		index = *list && (*list)->size > 0 && sort
		      ? get_dom_node_map_index(*list, node) : -1;

		if (!add_to_dom_node_list(list, node, index)) {
			done_dom_node(node);
			return NULL;
		}
	}

	return node;
}

void
done_dom_node_data(struct dom_node *node)
{
	union dom_node_data *data;

	assert(node);

	data = &node->data;

	switch (node->type) {
	case DOM_NODE_ATTRIBUTE:
		if (data->attribute.allocated)
			done_dom_string(&node->string);
		break;

	case DOM_NODE_DOCUMENT:
		if (data->document.element_ids)
			free_hash(data->document.element_ids);

		if (data->document.meta_nodes)
			done_dom_node_list(data->document.meta_nodes);

		if (data->document.children)
			done_dom_node_list(data->document.children);
		break;

	case DOM_NODE_ELEMENT:
		if (data->element.children)
			done_dom_node_list(data->element.children);

		if (data->element.map)
			done_dom_node_list(data->element.map);
		break;

	case DOM_NODE_TEXT:
		if (data->text.allocated)
			done_dom_string(&node->string);
		break;

	case DOM_NODE_PROCESSING_INSTRUCTION:
		if (data->proc_instruction.map)
			done_dom_node_list(data->proc_instruction.map);
		break;

	default:
		break;
	}

	mem_free(node);
}

void
done_dom_node(struct dom_node *node)
{
	assert(node);

	if (node->parent) {
		struct dom_node *parent = node->parent;
		union dom_node_data *data = &parent->data;

		switch (parent->type) {
		case DOM_NODE_DOCUMENT:
			del_from_dom_node_list(data->document.meta_nodes, node);
			del_from_dom_node_list(data->document.children, node);
			break;

		case DOM_NODE_ELEMENT:
			del_from_dom_node_list(data->element.children, node);
			del_from_dom_node_list(data->element.map, node);
			break;

		case DOM_NODE_PROCESSING_INSTRUCTION:
			del_from_dom_node_list(data->proc_instruction.map, node);
			break;

 		default:
 			break;
		}
 	}
 
	done_dom_node_data(node);
}

#define set_node_name(name, namelen, str)	\
	do { (name) = (str); (namelen) = sizeof(str) - 1; } while (0)

struct dom_string *
get_dom_node_name(struct dom_node *node)
{
	static struct dom_string cdata_section_str = INIT_DOM_STRING("#cdata-section", -1);
	static struct dom_string comment_str = INIT_DOM_STRING("#comment", -1);
	static struct dom_string document_str = INIT_DOM_STRING("#document", -1);
	static struct dom_string document_fragment_str = INIT_DOM_STRING("#document-fragment", -1);
	static struct dom_string text_str = INIT_DOM_STRING("#text", -1);

	assert(node);

	switch (node->type) {
	case DOM_NODE_CDATA_SECTION:
		return &cdata_section_str;

	case DOM_NODE_COMMENT:
		return &comment_str;

	case DOM_NODE_DOCUMENT:
		return &document_str;

	case DOM_NODE_DOCUMENT_FRAGMENT:
		return &document_fragment_str;

	case DOM_NODE_TEXT:
		return &text_str;

	case DOM_NODE_ATTRIBUTE:
	case DOM_NODE_DOCUMENT_TYPE:
	case DOM_NODE_ELEMENT:
	case DOM_NODE_ENTITY:
	case DOM_NODE_ENTITY_REFERENCE:
	case DOM_NODE_NOTATION:
	case DOM_NODE_PROCESSING_INSTRUCTION:
	default:
		return &node->string;
	}
}

struct dom_string *
get_dom_node_value(struct dom_node *node)
{
	assert(node);

	switch (node->type) {
	case DOM_NODE_ATTRIBUTE:
		return &node->data.attribute.value;

	case DOM_NODE_PROCESSING_INSTRUCTION:
		return &node->data.proc_instruction.instruction;

	case DOM_NODE_CDATA_SECTION:
	case DOM_NODE_COMMENT:
	case DOM_NODE_TEXT:
		return &node->string;

	case DOM_NODE_ENTITY_REFERENCE:
	case DOM_NODE_NOTATION:
	case DOM_NODE_DOCUMENT:
	case DOM_NODE_DOCUMENT_FRAGMENT:
	case DOM_NODE_DOCUMENT_TYPE:
	case DOM_NODE_ELEMENT:
	case DOM_NODE_ENTITY:
	default:
		return NULL;
	}
}

struct dom_string *
get_dom_node_type_name(enum dom_node_type type)
{
	static struct dom_string dom_node_type_names[DOM_NODES] = {
		INIT_DOM_STRING(NULL, 0),
		/* DOM_NODE_ELEMENT */			INIT_DOM_STRING("element", -1),
		/* DOM_NODE_ATTRIBUTE */		INIT_DOM_STRING("attribute", -1),
		/* DOM_NODE_TEXT */			INIT_DOM_STRING("text", -1),
		/* DOM_NODE_CDATA_SECTION */		INIT_DOM_STRING("cdata-section", -1),
		/* DOM_NODE_ENTITY_REFERENCE */		INIT_DOM_STRING("entity-reference", -1),
		/* DOM_NODE_ENTITY */			INIT_DOM_STRING("entity", -1),
		/* DOM_NODE_PROCESSING_INSTRUCTION */	INIT_DOM_STRING("proc-instruction", -1),
		/* DOM_NODE_COMMENT */			INIT_DOM_STRING("comment", -1),
		/* DOM_NODE_DOCUMENT */			INIT_DOM_STRING("document", -1),
		/* DOM_NODE_DOCUMENT_TYPE */		INIT_DOM_STRING("document-type", -1),
		/* DOM_NODE_DOCUMENT_FRAGMENT */	INIT_DOM_STRING("document-fragment", -1),
		/* DOM_NODE_NOTATION */			INIT_DOM_STRING("notation", -1),
	};

	assert(type < DOM_NODES);

	return &dom_node_type_names[type];
}
