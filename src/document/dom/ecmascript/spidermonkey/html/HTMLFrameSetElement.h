#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLFRAMESETELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLFRAMESETELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLFrameSetElement_class;
extern const JSFunctionSpec HTMLFrameSetElement_funcs[];
extern const JSPropertySpec HTMLFrameSetElement_props[];

struct FRAMESET_struct {
	struct HTMLElement_struct html;
	unsigned char *cols;
	unsigned char *rows;
};

void make_FRAMESET_object(JSContext *ctx, struct dom_node *node);
void done_FRAMESET_object(void *data);
#endif
