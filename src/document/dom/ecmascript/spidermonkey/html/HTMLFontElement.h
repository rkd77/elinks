#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLFONTELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLFONTELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLFontElement_class;
extern const JSFunctionSpec HTMLFontElement_funcs[];
extern const JSPropertySpec HTMLFontElement_props[];

struct FONT_struct {
	struct HTMLElement_struct html;
	unsigned char *color;
	unsigned char *face;
	unsigned char *size;
};

void make_FONT_object(JSContext *ctx, struct dom_node *node);
void done_FONT_object(void *data);
#endif
