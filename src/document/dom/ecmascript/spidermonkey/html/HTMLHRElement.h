#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLHRELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLHRELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLHRElement_class;
extern const JSFunctionSpec HTMLHRElement_funcs[];
extern const JSPropertySpec HTMLHRElement_props[];

struct HR_struct {
	struct HTMLElement_struct html;
	unsigned char *align;
	unsigned char *size;
	unsigned char *width;
	unsigned int no_shade:1;
};

void make_HR_object(JSContext *ctx, struct dom_node *node);
void done_HR_object(void *data);
#endif
