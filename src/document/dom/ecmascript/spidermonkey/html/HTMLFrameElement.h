#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLFRAMEELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLFRAMEELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLFrameElement_class;
extern const JSFunctionSpec HTMLFrameElement_funcs[];
extern const JSPropertySpec HTMLFrameElement_props[];

struct FRAME_struct {
	struct HTMLElement_struct html;
	unsigned char *frame_border;
	unsigned char *long_desc;
	unsigned char *margin_height;
	unsigned char *margin_width;
	unsigned char *name;
	unsigned char *scrolling;
	unsigned char *src;
	unsigned char *content_document; /* FIXME: proper type */
	unsigned int no_resize:1;
};

void make_FRAME_object(JSContext *ctx, struct dom_node *node);
void done_FRAME_object(struct dom_node *node);
#endif
