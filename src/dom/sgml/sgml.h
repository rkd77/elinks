#ifndef EL_DOM_SGML_SGML_H
#define EL_DOM_SGML_SGML_H

#include <stdlib.h>

#include "dom/node.h"
#include "dom/string.h"

/* The flags stored in the attribute sgml node info data */
/* TODO: Other potential flags (there can be only 16)
 *
 * - interaction info for forms (diabled, readonly, maxlength) maybe tabindex,
 * - table layout,
 * - generic layout style attributes maybe giving color values an additional flag,
 * - meta information (rel, rev, title, alt, summary, caption, standby, lang),
 * - scripting hooks (onblur, ...)
 * - information about the referenced content (hreflang, codetype, media, type)
 *
 * Anyway the flags should of course optimally have a purpose to speed things up
 * by quickly making it possible to identify certain attribute groups. --jonas */
enum sgml_attribute_flags {
	/* The value uniquely identifies the owner */
	SGML_ATTRIBUTE_IDENTIFIER = 1,
	/* The value contains an URI of some sort */
	SGML_ATTRIBUTE_REFERENCE = 2,
};

/* TODO: We also need an element flag to signal to the parser that all the
 * content should be skipped; possible with some ugly hacks to not use the
 * scanner since it could get confused. The purpose of this flag is to group
 * all element content in one text node. Kind of like a ``verbatim'' thing
 * where not parsing should be done. For HTML the <script> and <style> tags
 * should use it. */
enum sgml_element_flags {
	/* The start and end tags are optional */
	SGML_ELEMENT_OPTIONAL = 1,

	/* The element is empty and end tags are forbidden */
	SGML_ELEMENT_EMPTY = 2,

	/* The end tag is obtional */
	SGML_ELEMENT_END_OPTIONAL = 4,
};

struct sgml_node_info {
	struct dom_string string;
	uint16_t type;
	uint16_t flags;
};

/* The header node is special. It is used for storing the number of nodes and
 * for returning the default 'unknown' node. */
#define SGML_NODE_HEAD(doctype, nodetype) \
	{ INIT_DOM_STRING(NULL, doctype##_##nodetype##S - 1), doctype##_##nodetype##_UNKNOWN }

#define SGML_NODE_INFO(doctype, nodetype, name, data) \
	{ STATIC_DOM_STRING(#name), doctype##_##nodetype##_##name, data }

#define SGML_NODE_INF2(doctype, nodetype, name, ident, data) \
	{ STATIC_DOM_STRING(ident), doctype##_##nodetype##_##name, data }

#define SGML_NODE_INFO_TYPE(doctype, nodetype, name) doctype##_##nodetype##_##name

int sgml_info_strcmp(const void *key, const void *node);

static inline struct sgml_node_info *
get_sgml_node_info(struct sgml_node_info list[], struct dom_node *node)
{
	struct sgml_node_info *map = &list[1];
	size_t map_size = list->string.length;
	size_t obj_size = sizeof(struct sgml_node_info);
	void *match = bsearch(node, map, map_size, obj_size, sgml_info_strcmp);

	return match ? match : list;
}

enum sgml_document_type {
	SGML_DOCTYPE_DOCBOOK,
	SGML_DOCTYPE_HTML,
	SGML_DOCTYPE_RSS,
	SGML_DOCTYPE_XBEL,

	SGML_DOCTYPES,
};

struct sgml_info {
	enum sgml_document_type doctype;
	struct sgml_node_info *attributes;
	struct sgml_node_info *elements;
};

struct sgml_info *get_sgml_info(enum sgml_document_type doctype);

#endif
