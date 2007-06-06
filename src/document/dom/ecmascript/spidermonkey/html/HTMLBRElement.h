#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLBRELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLBRELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLBRElement_class;
extern const JSFunctionSpec HTMLBRElement_funcs[];
extern const JSPropertySpec HTMLBRElement_props[];

struct BR_struct {
	struct HTMLElement_struct html;
	unsigned char *clear;
};

void make_BR_object(JSContext *ctx, struct dom_node *node);
void done_BR_object(void *data);
#endif
