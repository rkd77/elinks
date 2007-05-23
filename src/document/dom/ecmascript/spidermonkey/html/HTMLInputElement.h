#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLINPUTELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLINPUTELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLInputElement_class;
extern const JSFunctionSpec HTMLInputElement_funcs[];
extern const JSPropertySpec HTMLInputElement_props[];

struct INPUT_struct {
	struct HTMLElement_struct html;
	unsigned char *default_value;
	unsigned char *default_checked;
	unsigned char *form; /* FIXME: proper type */
	unsigned char *accept;
	unsigned char *access_key;
	unsigned char *align;
	unsigned char *alt;
	unsigned char *checked;
	unsigned char *disabled;
	unsigned char *max_length;
	unsigned char *name;
	unsigned char *read_only;
	unsigned char *size;
	unsigned char *src;
	unsigned char *tab_index;
	unsigned char *type;
	unsigned char *use_map;
	unsigned char *value;
};

#endif
