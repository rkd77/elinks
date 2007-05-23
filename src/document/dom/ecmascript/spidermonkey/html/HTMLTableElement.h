#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTABLEELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTABLEELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLTableElement_class;
extern const JSFunctionSpec HTMLTableElement_funcs[];
extern const JSPropertySpec HTMLTableElement_props[];

struct TABLE_struct {
	struct HTMLElement_struct html;
	unsigned char *caption; /* FIXME: proper type */
	unsigned char *thead; /* FIXME: proper type */
	unsigned char *tfoot; /* FIXME: proper type */
	unsigned char *rows; /* FIXME: proper type */
	unsigned char *tbodies; /* FIXME: proper type */
	unsigned char *align;
	unsigned char *bgcolor;
	unsigned char *border;
	unsigned char *cell_padding;
	unsigned char *cell_spacing;
	unsigned char *frame;
	unsigned char *rules;
	unsigned char *summary;
	unsigned char *width;
};

#endif
