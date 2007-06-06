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
	unsigned char *src;
	unsigned char *type;
	unsigned int defer:1;
};

void make_SCRIPT_object(JSContext *ctx, struct dom_node *node);
void done_SCRIPT_object(void *data);
#endif
