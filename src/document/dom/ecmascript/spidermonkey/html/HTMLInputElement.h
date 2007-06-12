#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLINPUTELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLINPUTELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLInputElement_class;
extern const JSFunctionSpec HTMLInputElement_funcs[];
extern const JSPropertySpec HTMLInputElement_props[];

struct INPUT_struct {
	struct HTMLElement_struct html;
	struct dom_node *form; /* Must be first! */
	unsigned char *default_value;
	unsigned char *accept;
	unsigned char *access_key;
	unsigned char *align;
	unsigned char *alt;
	unsigned char *name;
	unsigned char *src;
	unsigned char *type;
	unsigned char *use_map;
	unsigned char *value;
	int max_length;
	int size;
	int tab_index;
	unsigned int default_checked:1;
	unsigned int checked:1;
	unsigned int disabled:1;
	unsigned int read_only:1;
};

void make_INPUT_object(JSContext *ctx, struct dom_node *node);
void done_INPUT_object(struct dom_node *node);
#endif
