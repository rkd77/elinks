#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_H

#include "document/dom/ecmascript/spidermonkey/props.h"

struct dom_node;
struct dom_string;

void done_dom_node_ecmascript_obj(struct dom_node *node);
void dom_add_attribute(struct dom_node *root, struct dom_node *parent, struct dom_string *attr, struct dom_string *value);

#endif
