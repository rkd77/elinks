#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLLEGENDELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLLEGENDELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLLegendElement_class;
extern const JSFunctionSpec HTMLLegendElement_funcs[];
extern const JSPropertySpec HTMLLegendElement_props[];

struct LEGEND_struct {
	struct HTMLElement_struct html;
	unsigned char *form; /* FIXME: proper type */
	unsigned char *access_key;
	unsigned char *align;
};

void make_LEGEND_object(JSContext *ctx, struct dom_node *node);
void done_LEGEND_object(void *data);
#endif
