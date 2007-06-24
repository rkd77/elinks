#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTML_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTML_H

#include "ecmascript/spidermonkey/util.h"
struct dom_node;

void done_dom_node_html_data(struct dom_node *node);
void make_dom_node_html_data(JSContext *ctx, struct dom_node *node);

#endif
