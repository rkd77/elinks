#ifndef EL_DOM_SGML_HTML_HTML_H
#define EL_DOM_SGML_HTML_HTML_H

#include "dom/sgml/sgml.h"

extern struct sgml_info sgml_html_info;

#undef VERSION
#define HTML_(node, name, flags)	SGML_NODE_INFO_TYPE(HTML, node, name)
#define HTM2_(node, name, str, flags)	SGML_NODE_INFO_TYPE(HTML, node, name)

enum html_element_type {
	HTML_ELEMENT_UNKNOWN,

#include "dom/sgml/html/element.inc"

	HTML_ELEMENTS,
};

enum html_attribute_type {
	HTML_ATTRIBUTE_UNKNOWN,

#include "dom/sgml/html/attribute.inc"

	HTML_ATTRIBUTES,
};

#undef	HTML_
#undef	HTM2_

#endif
