/* The MuJS Image implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/libdom/doc.h"
#include "js/ecmascript.h"
#include "js/libdom/dom.h"
#include "js/mujs.h"
#include "js/mujs/document.h"
#include "js/mujs/image.h"
#include "js/mujs/node.h"

static void
mjs_image_fun(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushundefined(J);
}

static void
mjs_image_constructor(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_document *doc = (dom_document *)mjs_doc_getprivate(J, 0);
	dom_string *tag_name = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)"img", 3, &tag_name);

	if (exc != DOM_NO_ERR || !tag_name) {
		js_pushnull(J);
		return;
	}
	dom_element *element = NULL;
	exc = dom_document_create_element(doc, tag_name, &element);
	dom_string_unref(tag_name);

	if (exc != DOM_NO_ERR || !element) {
		js_pushnull(J);
		return;
	}
	mjs_push_node(J, element);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(element);
}

int
mjs_image_init(js_State *J)
{
	js_pushglobal(J);
	js_newcconstructor(J, mjs_image_fun, mjs_image_constructor, "Image", 0);
	js_defglobal(J, "Image", JS_DONTENUM);

	return 0;
}
