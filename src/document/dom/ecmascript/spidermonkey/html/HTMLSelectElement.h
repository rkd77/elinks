#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLSELECTELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLSELECTELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLSelectElement_class;
extern const JSFunctionSpec HTMLSelectElement_funcs[];
extern const JSPropertySpec HTMLSelectElement_props[];

struct SELECT_struct {
	struct HTMLElement_struct html;
	unsigned char *type;
	unsigned char *value;
	unsigned char *form; /* FIXME: proper type */
	unsigned char *options; /* FIXME: proper type */
	unsigned char *name;
	int selected_index;
	int length;
	int size;
	int tab_index;
	unsigned int disabled:1;
	unsigned int multiple:1;
};

void make_SELECT_object(JSContext *ctx, struct dom_node *node);
void done_SELECT_object(void *data);
#endif
