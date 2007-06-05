#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLBODYELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLBODYELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLBodyElement_class;
extern const JSFunctionSpec HTMLBodyElement_funcs[];
extern const JSPropertySpec HTMLBodyElement_props[];

struct BODY_struct {
	struct HTMLElement_struct html;
	unsigned char *alink;
	unsigned char *background;
	unsigned char *bgcolor;
	unsigned char *link;
	unsigned char *text;
	unsigned char *vlink;
};

void make_BODY_object(JSContext *ctx, struct dom_node *node);
#endif
