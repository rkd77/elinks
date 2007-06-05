#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLDLISTELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLDLISTELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLDListElement_class;
extern const JSFunctionSpec HTMLDListElement_funcs[];
extern const JSPropertySpec HTMLDListElement_props[];

struct DL_struct {
	struct HTMLElement_struct html;
	unsigned int compact:1;
};

#endif
