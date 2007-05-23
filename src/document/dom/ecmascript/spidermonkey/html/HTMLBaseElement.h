#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLBASEELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLBASEELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLBaseElement_class;
extern const JSFunctionSpec HTMLBaseElement_funcs[];
extern const JSPropertySpec HTMLBaseElement_props[];

struct BASE_struct {
	struct HTMLElement_struct html;
	unsigned char *href;
	unsigned char *target;
};

#endif
