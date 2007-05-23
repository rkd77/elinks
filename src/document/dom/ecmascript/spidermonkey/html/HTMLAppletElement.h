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
	unsigned char *hspace;
	unsigned char *name;
	unsigned char *object;
	unsigned char *vspace;
	unsigned char *width;
};

#endif
