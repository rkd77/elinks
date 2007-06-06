#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLIFRAMEELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLIFRAMEELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLIFrameElement_class;
extern const JSFunctionSpec HTMLIFrameElement_funcs[];
extern const JSPropertySpec HTMLIFrameElement_props[];

struct IFRAME_struct {
	struct HTMLElement_struct html;
	unsigned char *align;
	unsigned char *frame_border;
	unsigned char *height;
	unsigned char *long_desc;
	unsigned char *margin_height;
	unsigned char *margin_width;
	unsigned char *name;
	unsigned char *scrolling;
	unsigned char *src;
	unsigned char *width;
	unsigned char *content_document; /* FIXME: proper type */
};

void make_IFRAME_object(JSContext *ctx, struct dom_node *node);
void done_IFRAME_object(void *data);
#endif
