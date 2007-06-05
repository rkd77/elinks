#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLOLISTELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLOLISTELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLOListElement_class;
extern const JSFunctionSpec HTMLOListElement_funcs[];
extern const JSPropertySpec HTMLOListElement_props[];

struct OL_struct {
	struct HTMLElement_struct html;
	unsigned char *type;
	int start;
	unsigned int compact:1;
};

#endif
