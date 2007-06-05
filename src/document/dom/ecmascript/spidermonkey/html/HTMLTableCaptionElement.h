#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTABLECAPTIONELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTABLECAPTIONELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLTableCaptionElement_class;
extern const JSFunctionSpec HTMLTableCaptionElement_funcs[];
extern const JSPropertySpec HTMLTableCaptionElement_props[];

struct CAPTION_struct {
	struct HTMLElement_struct html;
	unsigned char *align;
};

void make_CAPTION_object(JSContext *ctx, struct dom_node *node);
#endif
