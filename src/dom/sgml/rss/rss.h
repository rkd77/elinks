#ifndef EL_DOM_SGML_RSS_RSS_H
#define EL_DOM_SGML_RSS_RSS_H

#include "dom/sgml/sgml.h"

extern struct sgml_info sgml_rss_info;

#define RSS_(node, name, flags)	SGML_NODE_INFO_TYPE(RSS, node, name)

enum rss_element_type {
	RSS_ELEMENT_UNKNOWN,

#include "dom/sgml/rss/element.inc"

	RSS_ELEMENTS,
};

enum rss_attribute_type {
	RSS_ATTRIBUTE_UNKNOWN,

#include "dom/sgml/rss/attribute.inc"

	RSS_ATTRIBUTES,
};

#undef	RSS_

#endif
