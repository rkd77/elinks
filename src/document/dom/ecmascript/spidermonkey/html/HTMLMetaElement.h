#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLMETAELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLMETAELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLMetaElement_class;
extern const JSFunctionSpec HTMLMetaElement_funcs[];
extern const JSPropertySpec HTMLMetaElement_props[];

struct META_struct {
	struct HTMLElement_struct html;
	unsigned char *content;
	unsigned char *http_equiv;
	unsigned char *name;
	unsigned char *scheme;
};

void make_META_object(JSContext *ctx, struct dom_node *node);
void done_META_object(struct dom_node *node);
#endif
