#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTABLEROWELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTABLEROWELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLTableRowElement_class;
extern const JSFunctionSpec HTMLTableRowElement_funcs[];
extern const JSPropertySpec HTMLTableRowElement_props[];

struct TR_struct {
	struct HTMLElement_struct html;
	struct dom_node_list *cells;
	unsigned char *align;
	unsigned char *bgcolor;
	unsigned char *ch;
	unsigned char *ch_off;
	unsigned char *valign;
	int row_index;
	int section_row_index;
};

void make_TR_object(JSContext *ctx, struct dom_node *node);
void done_TR_object(struct dom_node *node);
void register_cell(struct dom_node *node);
void unregister_cell(struct dom_node *node);
#endif
