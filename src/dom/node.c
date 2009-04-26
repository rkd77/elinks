/* The DOM node handling */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "dom/node.h"
#include "dom/string.h"
#include "util/hash.h"
#include "util/memory.h"


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

int
get_dom_node_map_index(struct dom_node_list *list, struct dom_node *node)
{
	struct dom_node_search search = INIT_DOM_NODE_SEARCH(node, list);
	struct dom_node *match = dom_node_list_bsearch(&search, list);

	return match ? search.pos : search.to;
}

struct dom_node *
get_dom_node_map_entry(struct dom_node_list *list, enum dom_node_type type,
		       uint16_t subtype, struct dom_string *name)
{
	struct dom_node node = { type, 0, INIT_DOM_STRING(name->string, name->length) };
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

static int
get_dom_node_list_pos(struct dom_node_list *list, struct dom_node *node)
{
	struct dom_node *entry;
	int i;

	assert(list);
	if_assert_failed return -1;

	foreach_dom_node (list, entry, i) {
		if (entry == node)
			return i;
	}

	return -1;
}

int
get_dom_node_list_index(struct dom_node *parent, struct dom_node *node)
{
	struct dom_node_list **list = get_dom_node_list(parent, node);

	return (list && *list) ? get_dom_node_list_pos(*list, node) : -1;
}

struct dom_node *
get_dom_node_prev(struct dom_node *node)
{
	struct dom_node_list **list;
	int index;

	assert(node->parent);
	if_assert_failed return NULL;

	list = get_dom_node_list(node->parent, node);
	/* node->parent != NULL, so the node must be in the
	 * appropriate list of the parent; the list thus cannot be
	 * empty.  */
	if (!list) return NULL;
	assert(*list);
	if_assert_failed return NULL;

	index = get_dom_node_list_pos(*list, node);
	assert(index >= 0); /* in particular, not -1 */
	if_assert_failed return NULL;

	if (index > 0)
		return (*list)->entries[index - 1];

	return NULL;
}

struct dom_node *
get_dom_node_next(struct dom_node *node)
{
	struct dom_node_list **list;
	int index;

	assert(node->parent);
	if_assert_failed return NULL;

	list = get_dom_node_list(node->parent, node);
	/* node->parent != NULL, so the node must be in the
	 * appropriate list of the parent; the list thus cannot be
	 * empty.  */
	if (!list) return NULL;
	assert(*list);
	if_assert_failed return NULL;

	index = get_dom_node_list_pos(*list, node);
	assert(index >= 0); /* in particular, not -1 */
	if_assert_failed return NULL;

	if (index + 1 < (*list)->size)
		return (*list)->entries[index + 1];

	return NULL;
}

struct dom_node *
get_dom_node_child(struct dom_node *parent, enum dom_node_type type,
		   int16_t subtype)
{
	struct dom_node_list **list;
	struct dom_node *node;
	int index;

	list = get_dom_node_list_by_type(parent, type);
	if (!list) return NULL;	/* parent doesn't support this type */
	if (!*list) return NULL; /* list is empty and not yet allocated */

	foreach_dom_node (*list, node, index) {
		if (node->type != type)
			continue;

		if (!subtype) return node;

		switch (type) {
		case DOM_NODE_ELEMENT:
			if (node->data.element.type == subtype)
				return node;
			break;

		case DOM_NODE_ATTRIBUTE:
			if (node->data.attribute.type == subtype)
				return node;
			break;

		case DOM_NODE_PROCESSING_INSTRUCTION:
			if (node->data.attribute.type == subtype)
				return node;
			break;

		default:
			return node;
		}
	}

	return NULL;
}


/* Nodes */

struct dom_node *
init_dom_node_at(
#ifdef DEBUG_MEMLEAK
		unsigned char *file, int line,
#endif
		struct dom_node *parent, enum dom_node_type type,
		struct dom_string *string, int allocated)
{
#ifdef DEBUG_MEMLEAK
	struct dom_node *node = debug_mem_calloc(file, line, 1, sizeof(*node));
#else
	struct dom_node *node = mem_calloc(1, sizeof(*node));
#endif

	if (!node) return NULL;

	node->type   = type;
	node->parent = parent;

	/* Make it possible to add a node to a parent without allocating the
	 * strings. */
	if (allocated >= 0) {
		node->allocated = !!allocated;
	} else if (parent) {
		node->allocated = parent->allocated;
	}

	if (node->allocated) {
		if (!init_dom_string(&node->string, string->string, string->length)) {
			done_dom_node(node);
			return NULL;
		}
	} else {
		copy_dom_string(&node->string, string);
	}

	if (parent) {
		struct dom_node_list **list = get_dom_node_list(parent, node);
		int sort = (type == DOM_NODE_ATTRIBUTE);
		int index;

		assertm(list != NULL, "Adding node %d to bad parent %d",
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
	assert(node->type < DOM_NODES); /* unsigned comparison */

	data = &node->data;

	switch (node->type) {
	case DOM_NODE_ATTRIBUTE:
		if (node->allocated)
			done_dom_string(&data->attribute.value);
		break;

	case DOM_NODE_DOCUMENT:
		if (data->document.children)
			done_dom_node_list(data->document.children);
		break;

	case DOM_NODE_ELEMENT:
		if (data->element.children)
			done_dom_node_list(data->element.children);

		if (data->element.map)
			done_dom_node_list(data->element.map);
		break;

	case DOM_NODE_PROCESSING_INSTRUCTION:
		if (data->proc_instruction.map)
			done_dom_node_list(data->proc_instruction.map);
		if (node->allocated)
			done_dom_string(&data->proc_instruction.instruction);
		break;

	default:
		break;
	}

	if (node->allocated)
		done_dom_string(&node->string);

	/* call_dom_stack_callbacks() asserts that the node type is
	 * within range.  If assertions are enabled, store an
	 * out-of-range value there to make the assertion more likely
	 * to fail if a callback has freed the node prematurely.
	 * This is not entirely reliable though, because the memory
	 * is freed below and may be allocated for some other purpose
	 * before the assertion.  */
#ifndef CONFIG_FASTMEM
	node->type = -1;
#endif

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
	static struct dom_string cdata_section_str = STATIC_DOM_STRING("#cdata-section");
	static struct dom_string comment_str = STATIC_DOM_STRING("#comment");
	static struct dom_string document_str = STATIC_DOM_STRING("#document");
	static struct dom_string document_fragment_str = STATIC_DOM_STRING("#document-fragment");
	static struct dom_string text_str = STATIC_DOM_STRING("#text");

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
		/* DOM_NODE_ELEMENT */			STATIC_DOM_STRING("element"),
		/* DOM_NODE_ATTRIBUTE */		STATIC_DOM_STRING("attribute"),
		/* DOM_NODE_TEXT */			STATIC_DOM_STRING("text"),
		/* DOM_NODE_CDATA_SECTION */		STATIC_DOM_STRING("cdata-section"),
		/* DOM_NODE_ENTITY_REFERENCE */		STATIC_DOM_STRING("entity-reference"),
		/* DOM_NODE_ENTITY */			STATIC_DOM_STRING("entity"),
		/* DOM_NODE_PROCESSING_INSTRUCTION */	STATIC_DOM_STRING("proc-instruction"),
		/* DOM_NODE_COMMENT */			STATIC_DOM_STRING("comment"),
		/* DOM_NODE_DOCUMENT */			STATIC_DOM_STRING("document"),
		/* DOM_NODE_DOCUMENT_TYPE */		STATIC_DOM_STRING("document-type"),
		/* DOM_NODE_DOCUMENT_FRAGMENT */	STATIC_DOM_STRING("document-fragment"),
		/* DOM_NODE_NOTATION */			STATIC_DOM_STRING("notation"),
	};

	assert(type < DOM_NODES);

	return &dom_node_type_names[type];
}
