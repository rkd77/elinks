#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLPARAGRAPHELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLPARAGRAPHELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLParagraphElement_class;
extern const JSFunctionSpec HTMLParagraphElement_funcs[];
extern const JSPropertySpec HTMLParagraphElement_props[];

struct P_struct {
	struct HTMLElement_struct html;
	unsigned char *align;
};

void make_P_object(JSContext *ctx, struct dom_node *node);
void done_P_object(void *data);
#endif
