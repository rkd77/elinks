#ifndef EL__DOM_SGML_DOCBOOK_DOCBOOK_H
#define EL__DOM_SGML_DOCBOOK_DOCBOOK_H

#include "dom/stack.h"
#include "dom/sgml/sgml.h"

extern struct sgml_info sgml_docbook_info;

#define DOCBOOK_(node, name, flags) \
	SGML_NODE_INFO_TYPE(DOCBOOK, node, name)

enum docbook_element_type {
	DOCBOOK_ELEMENT_UNKNOWN,

#include "dom/sgml/docbook/element.inc"

	DOCBOOK_ELEMENTS,
};

enum docbook_attribute_type {
	DOCBOOK_ATTRIBUTE_UNKNOWN,

#include "dom/sgml/docbook/attribute.inc"

	DOCBOOK_ATTRIBUTES,
};

#undef	DOCBOOK_

#endif
