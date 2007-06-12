#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLOBJECTELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLOBJECTELEMENT_H

#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLObjectElement_class;
extern const JSFunctionSpec HTMLObjectElement_funcs[];
extern const JSPropertySpec HTMLObjectElement_props[];

struct OBJECT_struct {
	struct HTMLElement_struct html;
	struct dom_node *form; /* Must be first! */
	unsigned char *code;
	unsigned char *align;
	unsigned char *archive;
	unsigned char *border;
	unsigned char *code_base;
	unsigned char *code_type;
	unsigned char *data;
	unsigned char *height;
	unsigned char *name;
	unsigned char *standby;
	unsigned char *type;
	unsigned char *use_map;
	unsigned char *width;
	unsigned char *content_document; /* FIXME: proper type */
	int hspace;
	int tab_index;
	int vspace;
	unsigned int declare:1;
};

void make_OBJECT_object(JSContext *ctx, struct dom_node *node);
void done_OBJECT_object(struct dom_node *node);
#endif
