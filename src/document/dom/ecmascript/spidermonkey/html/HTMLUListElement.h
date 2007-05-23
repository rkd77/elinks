#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLULISTELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLULISTELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLUListElement_class;
extern const JSFunctionSpec HTMLUListElement_funcs[];
extern const JSPropertySpec HTMLUListElement_props[];

struct UL_struct {
	struct HTMLElement_struct html;
	unsigned char *compact;
	unsigned char *type;
};

#endif
