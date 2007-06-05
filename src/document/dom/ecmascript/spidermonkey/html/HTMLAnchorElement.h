#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLANCHORELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLANCHORELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLAnchorElement_class;
extern const JSFunctionSpec HTMLAnchorElement_funcs[];
extern const JSPropertySpec HTMLAnchorElement_props[];

struct A_struct {
	struct HTMLElement_struct html;
	unsigned char *access_key;
	unsigned char *charset;
	unsigned char *coords;
	unsigned char *href;
	unsigned char *hreflang;
	unsigned char *name;
	unsigned char *rel;
	unsigned char *rev;
	unsigned char *shape;
	unsigned char *target;
	unsigned char *type;
	int tab_index;
};

void make_A_object(JSContext *ctx, struct dom_node *node);
#endif
