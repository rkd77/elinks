/* RSS SGML info */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "dom/sgml/rss/rss.h"
#include "dom/sgml/sgml.h"


#define RSS_(node, name, id)	SGML_NODE_INFO(RSS, node, name, id)

static struct sgml_node_info rss_attributes[RSS_ATTRIBUTES] = {
	SGML_NODE_HEAD(RSS, ATTRIBUTE),

#include "dom/sgml/rss/attribute.inc"
};

static struct sgml_node_info rss_elements[RSS_ELEMENTS] = {
	SGML_NODE_HEAD(RSS, ELEMENT),

#include "dom/sgml/rss/element.inc"
};


struct sgml_info sgml_rss_info = {
	SGML_DOCTYPE_RSS,
	rss_attributes,
	rss_elements,
};
