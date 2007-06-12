#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLMENUELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLMENUELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLMenuElement_class;
extern const JSFunctionSpec HTMLMenuElement_funcs[];
extern const JSPropertySpec HTMLMenuElement_props[];

struct MENU_struct {
	struct HTMLElement_struct html;
	unsigned int compact:1;
};

void make_MENU_object(JSContext *ctx, struct dom_node *node);
void done_MENU_object(struct dom_node *node);
#endif
