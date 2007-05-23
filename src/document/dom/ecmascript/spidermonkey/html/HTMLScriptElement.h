#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLSCRIPTELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLSCRIPTELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLScriptElement_class;
extern const JSFunctionSpec HTMLScriptElement_funcs[];
extern const JSPropertySpec HTMLScriptElement_props[];

struct SCRIPT_struct {
	struct HTMLElement_struct html;
	unsigned char *text;
	unsigned char *html_for;
	unsigned char *event;
	unsigned char *charset;
	unsigned char *defer;
	unsigned char *src;
	unsigned char *type;
};

#endif
