#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLBASEFONTELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLBASEFONTELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLBaseFontElement_class;
extern const JSFunctionSpec HTMLBaseFontElement_funcs[];
extern const JSPropertySpec HTMLBaseFontElement_props[];

struct BASEFONT_struct {
	struct HTMLElement_struct html;
	unsigned char *color;
	unsigned char *face;
	int size;
};

void make_BASEFONT_object(JSContext *ctx, struct dom_node *node);
void done_BASEFONT_object(void *data);
#endif
