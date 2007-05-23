#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTEXTAREAELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTEXTAREAELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLTextAreaElement_class;
extern const JSFunctionSpec HTMLTextAreaElement_funcs[];
extern const JSPropertySpec HTMLTextAreaElement_props[];

struct TEXTAREA_struct {
	struct HTMLElement_struct html;
	unsigned char *default_value;
	unsigned char *form; /* FIXME: proper type */
	unsigned char *access_key;
	unsigned char *cols;
	unsigned char *disabled;
	unsigned char *name;
	unsigned char *read_only;
	unsigned char *rows;
	unsigned char *tab_index;
	unsigned char *type;
	unsigned char *value;

};

#endif
