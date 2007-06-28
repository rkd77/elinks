#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLDOCUMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLDOCUMENT_H

#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLDocument_class;
extern const JSFunctionSpec HTMLDocument_funcs[];
extern const JSPropertySpec HTMLDocument_props[];

struct dom_node;
struct dom_node_list;

struct HTMLDocument_struct {
	unsigned char *title;
	unsigned char *referrer;
	unsigned char *domain;
	unsigned char *URL;
	unsigned char *cookie;
	struct dom_node *body;
	struct dom_node_list *images;
	struct dom_node_list *applets;
	struct dom_node_list *links;
	struct dom_node_list *forms;
	struct dom_node_list *anchors;
};

#endif
