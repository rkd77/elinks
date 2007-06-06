#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLPARAMELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLPARAMELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLParamElement_class;
extern const JSFunctionSpec HTMLParamElement_funcs[];
extern const JSPropertySpec HTMLParamElement_props[];

struct PARAM_struct {
	struct HTMLElement_struct html;
	unsigned char *name;
	unsigned char *type;
	unsigned char *value;
	unsigned char *value_type;
};

void make_PARAM_object(JSContext *ctx, struct dom_node *node);
void done_PARAM_object(void *data);
#endif
