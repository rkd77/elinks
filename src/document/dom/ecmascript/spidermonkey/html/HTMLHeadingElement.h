#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLHEADINGELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLHEADINGELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLHeadingElement_class;
extern const JSFunctionSpec HTMLHeadingElement_funcs[];
extern const JSPropertySpec HTMLHeadingElement_props[];

struct H1_struct {
	struct HTMLElement_struct html;
	unsigned char *align;
};

#endif
