/* The MuJS Image implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CONFIG_LIBDOM
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>
#endif

#include "elinks.h"

#include "document/libdom/corestrings.h"
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_document *doc = (dom_document *)interpreter->doc;
	dom_element *element = NULL;
	dom_exception exc = dom_document_create_element(doc, corestring_dom_IMG, &element);

	if (exc != DOM_NO_ERR || !element) {
		js_pushnull(J);
		return;
	}
	const char *width = js_tostring(J, 1);

	if (width) {
		dom_string *value_str = NULL;
		exc = dom_string_create((const uint8_t *)width, strlen(width), &value_str);

		if (exc == DOM_NO_ERR) {
			(void)dom_element_set_attribute(element, corestring_dom_width, value_str);
			dom_string_unref(value_str);
		}
	}
	const char *height = js_tostring(J, 2);

	if (height) {
		dom_string *value_str = NULL;
		exc = dom_string_create((const uint8_t *)height, strlen(height), &value_str);

		if (exc == DOM_NO_ERR) {
			(void)dom_element_set_attribute(element, corestring_dom_height, value_str);
			dom_string_unref(value_str);
		}
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
