/* SGML node handling */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/sgml/html/html.h"
#include "document/sgml/sgml.h"


#define HTML_(node, name, id)		SGML_NODE_INFO(HTML, node, name, id)
#define HTM2_(node, name, str, id)	SGML_NODE_INF2(HTML, node, name, str, id)
#undef VERSION

static struct sgml_node_info html_attributes[HTML_ATTRIBUTES] = {
	SGML_NODE_HEAD(HTML, ATTRIBUTE),

#include "document/sgml/html/attribute.inc"
};

static struct sgml_node_info html_elements[HTML_ELEMENTS] = {
	SGML_NODE_HEAD(HTML, ELEMENT),

#include "document/sgml/html/element.inc"
};


struct sgml_info sgml_html_info = {
	SGML_DOCTYPE_HTML,
	html_attributes,
	html_elements,
};
