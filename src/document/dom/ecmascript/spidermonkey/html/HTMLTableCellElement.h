#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTABLECELLELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTABLECELLELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLTableCellElement_class;
extern const JSFunctionSpec HTMLTableCellElement_funcs[];
extern const JSPropertySpec HTMLTableCellElement_props[];

struct TD_struct {
	struct HTMLElement_struct html;
	unsigned char *abbr;
	unsigned char *align;
	unsigned char *axis;
	unsigned char *bgcolor;
	unsigned char *ch;
	unsigned char *ch_off;
	unsigned char *headers;
	unsigned char *height;
	unsigned char *scope;
	unsigned char *valign;
	unsigned char *width;
	int cell_index;
	int col_span;
	int row_span;
	unsigned int no_wrap:1;
};

void make_TD_object(JSContext *ctx, struct dom_node *node);
void done_TD_object(struct dom_node *node);
#endif
