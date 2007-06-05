#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLLIELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLLIELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLLIElement_class;
extern const JSFunctionSpec HTMLLIElement_funcs[];
extern const JSPropertySpec HTMLLIElement_props[];

struct LI_struct {
	struct HTMLElement_struct html;
	unsigned char *type;
	int value;
};

void make_LI_object(JSContext *ctx, struct dom_node *node);
#endif
