#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTITLEELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTITLEELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLTitleElement_class;
extern const JSFunctionSpec HTMLTitleElement_funcs[];
extern const JSPropertySpec HTMLTitleElement_props[];

struct TITLE_struct {
	struct HTMLElement_struct html;
	unsigned char *text;
};

#endif
