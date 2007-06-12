#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLFORMELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLFORMELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLFormElement_class;
extern const JSFunctionSpec HTMLFormElement_funcs[];
extern const JSPropertySpec HTMLFormElement_props[];

struct FORM_struct {
	struct HTMLElement_struct html;
	struct dom_node_list *elements;
	unsigned char *name;
	unsigned char *accept_charset;
	unsigned char *action;
	unsigned char *enctype;
	unsigned char *method;
	unsigned char *target;
	int length;
};

void make_FORM_object(JSContext *ctx, struct dom_node *node);
void done_FORM_object(void *data);
void register_form_element(struct dom_node *form, struct dom_node *node);
void unregister_form_element(struct dom_node *form, struct dom_node *node);
struct dom_node *find_parent_form(struct dom_node *node);

#endif
