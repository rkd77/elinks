#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLAPPLETELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLAPPLETELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLAppletElement_class;
extern const JSFunctionSpec HTMLAppletElement_funcs[];
extern const JSPropertySpec HTMLAppletElement_props[];

struct APPLET_struct {
	struct HTMLElement_struct html;
	unsigned char *align;
	unsigned char *alt;
	unsigned char *archive;
	unsigned char *code;
	unsigned char *code_base;
	unsigned char *height;
	unsigned char *name;
	unsigned char *object;
	unsigned char *width;
	int hspace;
	int vspace;
};

void make_APPLET_object(JSContext *ctx, struct dom_node *node);
void done_APPLET_object(struct dom_node *node);
#endif
