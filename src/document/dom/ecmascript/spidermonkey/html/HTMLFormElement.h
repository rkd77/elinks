#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLFORMELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLFORMELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLFormElement_class;
extern const JSFunctionSpec HTMLFormElement_funcs[];
extern const JSPropertySpec HTMLFormElement_props[];

struct FORM_struct {
	struct HTMLElement_struct html;
	unsigned char *elements; /* FIXME: proper type */
	unsigned char *name;
	unsigned char *accept_charset;
	unsigned char *action;
	unsigned char *enctype;
	unsigned char *method;
	unsigned char *target;
	int length;
};

void make_FORM_object(JSContext *ctx, struct dom_node *node);
#endif
