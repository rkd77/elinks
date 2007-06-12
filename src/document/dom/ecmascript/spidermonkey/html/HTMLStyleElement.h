#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLSTYLEELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLSTYLEELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLStyleElement_class;
extern const JSFunctionSpec HTMLStyleElement_funcs[];
extern const JSPropertySpec HTMLStyleElement_props[];

struct STYLE_struct {
	struct HTMLElement_struct html;
	unsigned char *media;
	unsigned char *type;
	unsigned int disabled:1;
};

void make_STYLE_object(JSContext *ctx, struct dom_node *node);
void done_STYLE_object(struct dom_node *node);
#endif
