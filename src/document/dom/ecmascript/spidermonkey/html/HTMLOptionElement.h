#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLOPTIONELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLOPTIONELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLOptionElement_class;
extern const JSFunctionSpec HTMLOptionElement_funcs[];
extern const JSPropertySpec HTMLOptionElement_props[];

struct OPTION_struct {
	struct HTMLElement_struct html;
	unsigned char *form; /* FIXME: proper type */
	unsigned char *text;
	unsigned char *label;
	unsigned char *value;
	int index;
	unsigned int default_selected:1;
	unsigned int disabled:1;
	unsigned int selected:1;
};

void make_OPTION_object(JSContext *ctx, struct dom_node *node);
void done_OPTION_object(void *data);
#endif
