#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLISINDEXELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLISINDEXELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLIsIndexElement_class;
extern const JSFunctionSpec HTMLIsIndexElement_funcs[];
extern const JSPropertySpec HTMLIsIndexElement_props[];

struct ISINDEX_struct {
	struct HTMLElement_struct html;
	unsigned char *form; /* FIXME: proper type */
	unsigned char *prompt;
};

#endif
