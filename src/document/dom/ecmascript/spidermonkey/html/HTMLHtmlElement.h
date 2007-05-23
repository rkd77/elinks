#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLHTMLELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLHTMLELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLHtmlElement_class;
extern const JSFunctionSpec HTMLHtmlElement_funcs[];
extern const JSPropertySpec HTMLHtmlElement_props[];

struct HTML_struct {
	struct HTMLElement_struct html;
	unsigned char *version;
};

#endif
