#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLDIVELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLDIVELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLDivElement_class;
extern const JSFunctionSpec HTMLDivElement_funcs[];
extern const JSPropertySpec HTMLDivElement_props[];

struct DIV_struct {
	struct HTMLElement_struct html;
	unsigned char *align;
};

#endif
