/* The DOM node handling */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/dom/node.h"
#include "document/options.h"
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

	foreach_dom_node(i, entry, list) {
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

	foreach_dom_node (i, node, list) {
		/* Avoid that the node start messing with the node list. */
		done_dom_node_data(node);
	}

	mem_free(list);
}


/* Node map */

struct dom_node_search {
	struct dom_node *key;
	int subtype;
	unsigned int from, pos, to;
};

#define INIT_DOM_NODE_SEARCH(key, subtype, list) \
	{ (key), (subtype), -1, 0, (list)->size, }

static inline int
dom_node_cmp(struct dom_node_search *search, struct dom_node *node)
{
	struct dom_node *key = search->key;

	if (search->subtype) {
		assert(key->type == node->type);

		switch (key->type) {
		case DOM_NODE_ELEMENT:
			return search->subtype - node->data.element.type;

		case DOM_NODE_ATTRIBUTE:
			return search->subtype - node->data.attribute.type;

		default:
			break;
		}
	}
	{
		int length = int_min(key->length, node->length);
		int string_diff = strncasecmp(key->string, node->string, length);

		/* If the lengths or strings don't match strncasecmp() does the
		 * job else return which ever is bigger. */

		return string_diff ? string_diff : key->length - node->length;
	}
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
		int difference = dom_node_cmp(search, node);

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
	struct dom_node_search search = INIT_DOM_NODE_SEARCH(node, 0, list);
	struct dom_node *match = dom_node_list_bsearch(&search, list);

	return match ? search.pos : search.to;
}

struct dom_node *
get_dom_node_map_entry(struct dom_node_list *list, enum dom_node_type type,
		       uint16_t subtype, unsigned char *name, int namelen)
{
	struct dom_node node = { type, namelen, name };
	struct dom_node_search search = INIT_DOM_NODE_SEARCH(&node, subtype, list);

	return dom_node_list_bsearch(&search, list);
}

/* Nodes */

struct dom_node *
init_dom_node_(unsigned char *file, int line,
		struct dom_node *parent, enum dom_node_type type,
		unsigned char *string, uint16_t length)
{
#ifdef DEBUG_MEMLEAK
	struct dom_node *node = debug_mem_calloc(file, line, 1, sizeof(*node));
#else
	struct dom_node *node = mem_calloc(1, sizeof(*node));
#endif

	if (!node) return NULL;

	node->type   = type;
	node->string = string;
	node->length = length;
	node->parent = parent;

	if (parent) {
		struct dom_node_list **list = get_dom_node_list(parent, node);
		int sort = (type == DOM_NODE_ATTRIBUTE);
		int index;

		assertm(list, "Adding %s to bad parent %s",
			get_dom_node_type_name(node->type),
			get_dom_node_type_name(parent->type));

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
			mem_free(node->string);
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
			mem_free(node->string);
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

unsigned char *
get_dom_node_name(struct dom_node *node)
{
	unsigned char *name;
	uint16_t namelen;

	assert(node);

	switch (node->type) {
		case DOM_NODE_CDATA_SECTION:
			set_node_name(name, namelen, "#cdata-section");
			break;

		case DOM_NODE_COMMENT:
			set_node_name(name, namelen, "#comment");
			break;

		case DOM_NODE_DOCUMENT:
			set_node_name(name, namelen, "#document");
			break;

		case DOM_NODE_DOCUMENT_FRAGMENT:
			set_node_name(name, namelen, "#document-fragment");
			break;

		case DOM_NODE_TEXT:
			set_node_name(name, namelen, "#text");
			break;

		case DOM_NODE_ATTRIBUTE:
		case DOM_NODE_DOCUMENT_TYPE:
		case DOM_NODE_ELEMENT:
		case DOM_NODE_ENTITY:
		case DOM_NODE_ENTITY_REFERENCE:
		case DOM_NODE_NOTATION:
		case DOM_NODE_PROCESSING_INSTRUCTION:
		default:
			name	= node->string;
			namelen	= node->length;
	}

	return memacpy(name, namelen);
}

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

unsigned char *
get_dom_node_value(struct dom_node *node, int codepage)
{
	unsigned char *value;
	uint16_t valuelen;

	assert(node);

	switch (node->type) {
		case DOM_NODE_ATTRIBUTE:
			value	 = node->data.attribute.value;
			valuelen = node->data.attribute.valuelen;
			break;

		case DOM_NODE_PROCESSING_INSTRUCTION:
			value	 = node->data.proc_instruction.instruction;
			valuelen = node->data.proc_instruction.instructionlen;
			break;

		case DOM_NODE_CDATA_SECTION:
		case DOM_NODE_COMMENT:
		case DOM_NODE_TEXT:
			value	 = node->string;
			valuelen = node->length;
			break;

		case DOM_NODE_ENTITY_REFERENCE:
			value = get_entity_string(node->string, node->length,
						  codepage);
			valuelen = value ? strlen(value) : 0;
			break;

		case DOM_NODE_NOTATION:
		case DOM_NODE_DOCUMENT:
		case DOM_NODE_DOCUMENT_FRAGMENT:
		case DOM_NODE_DOCUMENT_TYPE:
		case DOM_NODE_ELEMENT:
		case DOM_NODE_ENTITY:
		default:
			return NULL;
	}

	if (!value) value = "";

	return compress_string(value, valuelen);
}

unsigned char *
get_dom_node_type_name(enum dom_node_type type)
{
	static unsigned char *dom_node_type_names[DOM_NODES] = {
		NULL,
		/* DOM_NODE_ELEMENT */			"element",
		/* DOM_NODE_ATTRIBUTE */		"attribute",
		/* DOM_NODE_TEXT */			"text",
		/* DOM_NODE_CDATA_SECTION */		"cdata-section",
		/* DOM_NODE_ENTITY_REFERENCE */		"entity-reference",
		/* DOM_NODE_ENTITY */			"entity",
		/* DOM_NODE_PROCESSING_INSTRUCTION */	"proc-instruction",
		/* DOM_NODE_COMMENT */			"comment",
		/* DOM_NODE_DOCUMENT */			"document",
		/* DOM_NODE_DOCUMENT_TYPE */		"document-type",
		/* DOM_NODE_DOCUMENT_FRAGMENT */	"document-fragment",
		/* DOM_NODE_NOTATION */			"notation",
	};

	assert(type < DOM_NODES);

	return dom_node_type_names[type];
}
