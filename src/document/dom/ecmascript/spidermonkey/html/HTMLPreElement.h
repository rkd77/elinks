#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLPREELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLPREELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLPreElement_class;
extern const JSFunctionSpec HTMLPreElement_funcs[];
extern const JSPropertySpec HTMLPreElement_props[];

struct PRE_struct {
	struct HTMLElement_struct html;
	int width;
};

void make_PRE_object(JSContext *ctx, struct dom_node *node);
void done_PRE_object(void *data);
#endif
