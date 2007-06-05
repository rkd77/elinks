#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTABLECOLELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTABLECOLELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLTableColElement_class;
extern const JSFunctionSpec HTMLTableColElement_funcs[];
extern const JSPropertySpec HTMLTableColElement_props[];

struct COL_struct {
	struct HTMLElement_struct html;
	unsigned char *align;
	unsigned char *ch;
	unsigned char *ch_off;
	unsigned char *valign;
	unsigned char *width;
	int span;
};

void make_COL_object(JSContext *ctx, struct dom_node *node);
#endif
