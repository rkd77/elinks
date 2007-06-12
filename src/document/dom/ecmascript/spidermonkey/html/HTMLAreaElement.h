#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLAREAELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLAREAELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLAreaElement_class;
extern const JSFunctionSpec HTMLAreaElement_funcs[];
extern const JSPropertySpec HTMLAreaElement_props[];

struct AREA_struct {
	struct HTMLElement_struct html;
	unsigned char *access_key;
	unsigned char *alt;
	unsigned char *coords;
	unsigned char *href;
	unsigned char *shape;
	unsigned char *target;
	int tab_index;
	unsigned int no_href:1;
};

void make_AREA_object(JSContext *ctx, struct dom_node *node);
void done_AREA_object(struct dom_node *node);
#endif
