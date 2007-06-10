#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLIMAGEELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLIMAGEELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLImageElement_class;
extern const JSFunctionSpec HTMLImageElement_funcs[];
extern const JSPropertySpec HTMLImageElement_props[];

struct IMG_struct {
	struct HTMLElement_struct html;
	unsigned char *name;
	unsigned char *align;
	unsigned char *alt;
	unsigned char *border;
	unsigned char *long_desc;
	unsigned char *src;
	unsigned char *use_map;
	int height;
	int hspace;
	int vspace;
	int width;
	unsigned int is_map:1;
};

void make_IMG_object(JSContext *ctx, struct dom_node *node);
void done_IMG_object(void *data);
#endif
