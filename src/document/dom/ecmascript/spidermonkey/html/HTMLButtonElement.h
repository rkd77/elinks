#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLBUTTONELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLBUTTONELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLButtonElement_class;
extern const JSFunctionSpec HTMLButtonElement_funcs[];
extern const JSPropertySpec HTMLButtonElement_props[];

struct BUTTON_struct {
	struct HTMLElement_struct html;
	unsigned char *form; /* TODO: proper type */
	unsigned char *access_key;
	unsigned char *name;
	unsigned char *type;
	unsigned char *value;
	int tab_index;
	unsigned char disabled:1;
};

void make_BUTTON_object(JSContext *ctx, struct dom_node *node);
void done_BUTTON_object(void *data);
#endif
