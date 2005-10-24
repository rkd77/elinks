
#ifndef EL__DOCUMENT_SGML_HTML_HTML_H
#define EL__DOCUMENT_SGML_HTML_HTML_H

#include "document/dom/navigator.h"
#include "document/sgml/sgml.h"

extern struct sgml_info sgml_html_info;

#undef VERSION
#define HTML_NODE_INFO(node, name, flags)	SGML_NODE_INFO_TYPE(HTML, node, name)
#define HTML_NODE_INF2(node, name, str, flags)	SGML_NODE_INFO_TYPE(HTML, node, name)

enum html_element_type {
	HTML_ELEMENT_UNKNOWN,

#include "document/sgml/html/element.inc"

	HTML_ELEMENTS,
};

enum html_attribute_type {
	HTML_ATTRIBUTE_UNKNOWN,

#include "document/sgml/html/attribute.inc"

	HTML_ATTRIBUTES,
};

#undef	HTML_NODE_INFO
#undef	HTML_NODE_INF2

#endif
