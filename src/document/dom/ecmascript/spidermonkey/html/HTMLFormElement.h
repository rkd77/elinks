#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLFORMELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLFORMELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLFormElement_class;
extern const JSFunctionSpec HTMLFormElement_funcs[];
extern const JSPropertySpec HTMLFormElement_props[];

struct FRAME_struct {
	struct HTMLElement_struct html;
	unsigned char *frame_border;
	unsigned char *long_desc;
	unsigned char *margin_height;
	unsigned char *margin_width;
	unsigned char *name;
	unsigned char *no_resize;
	unsigned char *scrolling;
	unsigned char *src;
	unsigned char *content_document; /* FIXME: proper type */
};

#endif
