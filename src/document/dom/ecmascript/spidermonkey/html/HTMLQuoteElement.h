#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLQUOTEELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLQUOTEELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLQuoteElement_class;
extern const JSFunctionSpec HTMLQuoteElement_funcs[];
extern const JSPropertySpec HTMLQuoteElement_props[];

struct QUOTE_struct {
	struct HTMLElement_struct html;
	unsigned char *cite;
};

void make_QUOTE_object(JSContext *ctx, struct dom_node *node);
#endif
