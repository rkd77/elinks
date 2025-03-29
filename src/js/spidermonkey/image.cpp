/* The SpiderMonkey Image object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/libdom/dom.h"
#include "document/libdom/corestrings.h"

#include "js/spidermonkey/util.h"
#include <js/BigInt.h>
#include <js/Conversions.h>

#include "js/ecmascript.h"
#include "js/spidermonkey.h"
#include "js/spidermonkey/document.h"
#include "js/spidermonkey/image.h"
#include "js/spidermonkey/node.h"

JSClassOps image_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	nullptr, // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook // trace
};

JSClass image_class = {
	"Image",
	JSCLASS_HAS_RESERVED_SLOTS(0),
	&image_ops
};

bool
image_constructor(JSContext* ctx, unsigned argc, JS::Value* vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	JSObject *doc_obj = (JSObject *)(interpreter->document_obj);
	JS::RootedObject hobj(ctx, doc_obj);

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_document *doc = JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);
	NODEINFO(doc);
	dom_element *element = NULL;
	dom_exception exc = dom_document_create_element(doc, corestring_dom_IMG, &element);

	if (exc != DOM_NO_ERR || !element) {
		args.rval().setNull();
		return true;
	}

	if (argc > 0) {
		char *width = jsval_to_string(ctx, args[0]);

		if (width) {
			size_t width_len = strlen(width);
			dom_string *value_str = NULL;

			exc = dom_string_create((const uint8_t *)width, width_len, &value_str);
			mem_free(width);

			if (exc == DOM_NO_ERR) {
				(void)dom_element_set_attribute(element, corestring_dom_width, value_str);
				dom_string_unref(value_str);
			}
		}
	}

	if (argc > 1) {
		char *height = jsval_to_string(ctx, args[1]);

		if (height) {
			size_t height_len = strlen(height);
			dom_string *value_str = NULL;

			exc = dom_string_create((const uint8_t *)height, height_len, &value_str);
			mem_free(height);

			if (exc == DOM_NO_ERR) {
				(void)dom_element_set_attribute(element, corestring_dom_height, value_str);
				dom_string_unref(value_str);
			}
		}
	}
	JSObject *obj = getNode(ctx, element);
	args.rval().setObject(*obj);

	return true;
}
