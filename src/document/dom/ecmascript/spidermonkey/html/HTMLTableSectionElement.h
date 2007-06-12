#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTABLESECTIONELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTABLESECTIONELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLTableSectionElement_class;
extern const JSFunctionSpec HTMLTableSectionElement_funcs[];
extern const JSPropertySpec HTMLTableSectionElement_props[];

struct THEAD_struct {
	struct HTMLElement_struct html;
	struct dom_node_list *rows;
	unsigned char *align;
	unsigned char *ch;
	unsigned char *chOff;
	unsigned char *vAlign;
};

void make_THEAD_object(JSContext *ctx, struct dom_node *node);
void done_THEAD_object(struct dom_node *node);
void register_section_row(struct dom_node *node);
void unregister_section_row(struct dom_node *node);

#define make_TBODY_object	make_THEAD_object
#define make_TFOOT_object	make_THEAD_object
#define done_TBODY_object	done_THEAD_object
#define done_TFOOT_object	done_THEAD_object

#endif
