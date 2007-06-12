#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTABLEELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTABLEELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLTableElement_class;
extern const JSFunctionSpec HTMLTableElement_funcs[];
extern const JSPropertySpec HTMLTableElement_props[];

struct TABLE_struct {
	struct HTMLElement_struct html;
	struct dom_node *caption;
	struct dom_node *thead;
	struct dom_node *tfoot;
	struct dom_node_list *rows;
	struct dom_node_list *tbodies;
	unsigned char *align;
	unsigned char *bgcolor;
	unsigned char *border;
	unsigned char *cell_padding;
	unsigned char *cell_spacing;
	unsigned char *frame;
	unsigned char *rules;
	unsigned char *summary;
	unsigned char *width;
};

void make_TABLE_object(JSContext *ctx, struct dom_node *node);
void done_TABLE_object(struct dom_node *node);
struct dom_node *find_parent_table(struct dom_node *node);
void register_row(struct dom_node *node);
void unregister_row(struct dom_node *node);
void register_tbody(struct dom_node *table, struct dom_node *node);
void unregister_tbody(struct dom_node *node);

#endif
