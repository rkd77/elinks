/* XBEL SGML info */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "dom/sgml/xbel/xbel.h"
#include "dom/sgml/sgml.h"


#define XBEL_(node, name, id)	SGML_NODE_INFO(XBEL, node, name, id)

static struct sgml_node_info xbel_attributes[XBEL_ATTRIBUTES] = {
	SGML_NODE_HEAD(XBEL, ATTRIBUTE),

#include "dom/sgml/xbel/attribute.inc"
};

static struct sgml_node_info xbel_elements[XBEL_ELEMENTS] = {
	SGML_NODE_HEAD(XBEL, ELEMENT),

#include "dom/sgml/xbel/element.inc"
};


struct sgml_info sgml_xbel_info = {
	SGML_DOCTYPE_XBEL,
	xbel_attributes,
	xbel_elements,
};
