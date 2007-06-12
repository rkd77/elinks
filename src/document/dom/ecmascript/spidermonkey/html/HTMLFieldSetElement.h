#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLFIELDSETELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLFIELDSETELEMENT_H

#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLFieldSetElement_class;
extern const JSFunctionSpec HTMLFieldSetElement_funcs[];
extern const JSPropertySpec HTMLFieldSetElement_props[];

struct FIELDSET_struct {
	struct HTMLElement_struct html;
	struct dom_node *form; /* Must be first! */
};

void make_FIELDSET_object(JSContext *ctx, struct dom_node *node);
void done_FIELDSET_object(struct dom_node *node);

#endif
