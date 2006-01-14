/* DocBook SGML info */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "dom/sgml/docbook/docbook.h"
#include "dom/sgml/sgml.h"


#define DOCBOOK_(node, name, id) \
	SGML_NODE_INFO(DOCBOOK, node, name, id)

static struct sgml_node_info docbook_attributes[DOCBOOK_ATTRIBUTES] = {
	SGML_NODE_HEAD(DOCBOOK, ATTRIBUTE),

#include "dom/sgml/docbook/attribute.inc"
};

static struct sgml_node_info docbook_elements[DOCBOOK_ELEMENTS] = {
	SGML_NODE_HEAD(DOCBOOK, ELEMENT),

#include "dom/sgml/docbook/element.inc"
};


struct sgml_info sgml_docbook_info = {
	SGML_DOCTYPE_DOCBOOK,
	docbook_attributes,
	docbook_elements,
};
