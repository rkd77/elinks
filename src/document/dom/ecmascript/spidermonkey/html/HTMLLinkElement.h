#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLLINKELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLLINKELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLLinkElement_class;
extern const JSFunctionSpec HTMLLinkElement_funcs[];
extern const JSPropertySpec HTMLLinkElement_props[];

struct LINK_struct {
	struct HTMLElement_struct html;
	unsigned char *disabled;
	unsigned char *charset;
	unsigned char *href;
	unsigned char *hreflang;
	unsigned char *media;
	unsigned char *rel;
	unsigned char *rev;
	unsigned char *target;
	unsigned char *type;
};

#endif
