#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTABLEROWELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLTABLEROWELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLTableRowElement_class;
extern const JSFunctionSpec HTMLTableRowElement_funcs[];
extern const JSPropertySpec HTMLTableRowElement_props[];

struct TR_struct {
	struct HTMLElement_struct html;
	unsigned char *cells; /* FIXME: proper type */
	unsigned char *align;
	unsigned char *bgcolor;
	unsigned char *ch;
	unsigned char *ch_off;
	unsigned char *valign;
	int row_index;
	int section_row_index;
};

#endif
