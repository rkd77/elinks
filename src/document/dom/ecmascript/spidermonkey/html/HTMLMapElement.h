#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLMAPELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLMAPELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLMapElement_class;
extern const JSFunctionSpec HTMLMapElement_funcs[];
extern const JSPropertySpec HTMLMapElement_props[];

struct MAP_struct {
	struct HTMLElement_struct html;
	unsigned char *areas; /* FIXME: proper type */
	unsigned char *name; 
};

void make_MAP_object(JSContext *ctx, struct dom_node *node);
void done_MAP_object(void *data);
#endif
