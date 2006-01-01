#ifndef EL_DOM_SGML_XBEL_XBEL_H
#define EL_DOM_SGML_XBEL_XBEL_H

#include "dom/sgml/sgml.h"

extern struct sgml_info sgml_xbel_info;

#define XBEL_(node, name, flags)	SGML_NODE_INFO_TYPE(XBEL, node, name)

enum xbel_element_type {
	XBEL_ELEMENT_UNKNOWN,

#include "dom/sgml/xbel/element.inc"

	XBEL_ELEMENTS,
};

enum xbel_attribute_type {
	XBEL_ATTRIBUTE_UNKNOWN,

#include "dom/sgml/xbel/attribute.inc"

	XBEL_ATTRIBUTES,
};

#undef	XBEL_

#endif
