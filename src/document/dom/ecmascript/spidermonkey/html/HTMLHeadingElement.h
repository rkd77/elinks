#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLHEADINGELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLHEADINGELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLHeadingElement_class;
extern const JSFunctionSpec HTMLHeadingElement_funcs[];
extern const JSPropertySpec HTMLHeadingElement_props[];

struct H1_struct {
	struct HTMLElement_struct html;
	unsigned char *align;
};

void make_H1_object(JSContext *ctx, struct dom_node *node);
void done_H1_object(void *data);

#define make_H2_object	make_H1_object
#define make_H3_object	make_H1_object
#define make_H4_object	make_H1_object
#define make_H5_object	make_H1_object
#define make_H6_object	make_H1_object

#define done_H2_object	done_H1_object
#define done_H3_object	done_H1_object
#define done_H4_object	done_H1_object
#define done_H5_object	done_H1_object
#define done_H6_object	done_H1_object

#endif
