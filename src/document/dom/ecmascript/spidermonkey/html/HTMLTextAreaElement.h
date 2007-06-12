#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTEXTAREAELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTEXTAREAELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLTextAreaElement_class;
extern const JSFunctionSpec HTMLTextAreaElement_funcs[];
extern const JSPropertySpec HTMLTextAreaElement_props[];

struct TEXTAREA_struct {
	struct HTMLElement_struct html;
	struct dom_node *form; /* Must be first! */
	unsigned char *default_value;
	unsigned char *access_key;
	unsigned char *name;
	unsigned char *type;
	unsigned char *value;
	int cols;
	int rows;
	int tab_index;
	unsigned int disabled:1;
	unsigned int read_only:1;
};

void make_TEXTAREA_object(JSContext *ctx, struct dom_node *node);
void done_TEXTAREA_object(struct dom_node *node);

#endif
