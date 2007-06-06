#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLLABELELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLLABELELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLLabelElement_class;
extern const JSFunctionSpec HTMLLabelElement_funcs[];
extern const JSPropertySpec HTMLLabelElement_props[];

struct LABEL_struct {
	struct HTMLElement_struct html;
	unsigned char *form; /* FIXME: proper type */
	unsigned char *access_key;
	unsigned char *html_for;
};

void make_LABEL_object(JSContext *ctx, struct dom_node *node);
void done_LABEL_object(void *data);
#endif
