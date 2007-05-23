#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLSELECTELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLSELECTELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLSelectElement_class;
extern const JSFunctionSpec HTMLSelectElement_funcs[];
extern const JSPropertySpec HTMLSelectElement_props[];

struct SELECT_struct {
	struct HTMLElement_struct html;
	unsigned char *type;
	unsigned char *selected_index;
	unsigned char *value;
	unsigned char *length;
	unsigned char *form; /* FIXME: proper type */
	unsigned char *options; /* FIXME: proper type */
	unsigned char *disabled;
	unsigned char *multiple;
	unsigned char *name;
	unsigned char *size;
	unsigned char *tab_index;
};

#endif
