#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLOPTIONELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLOPTIONELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLOptionElement_class;
extern const JSFunctionSpec HTMLOptionElement_funcs[];
extern const JSPropertySpec HTMLOptionElement_props[];

struct OPTION_struct {
	struct HTMLElement_struct html;
	unsigned char *form; /* FIXME: proper type */
	unsigned char *default_selected;
	unsigned char *text;
	unsigned char *index;
	unsigned char *disabled;
	unsigned char *label;
	unsigned char *selected;
	unsigned char *value;
};

#endif
