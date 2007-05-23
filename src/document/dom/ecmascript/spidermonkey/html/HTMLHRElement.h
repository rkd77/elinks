#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLHRELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLHRELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLHRElement_class;
extern const JSFunctionSpec HTMLHRElement_funcs[];
extern const JSPropertySpec HTMLHRElement_props[];

struct HR_struct {
	struct HTMLElement_struct html;
	unsigned char *align;
	unsigned char *no_shade;
	unsigned char *size;
	unsigned char *width;
};

#endif
