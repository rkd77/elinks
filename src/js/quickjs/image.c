/* The QuickJS Image object implementation. */

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

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/libdom/corestrings.h"
#include "document/view.h"
#include "js/ecmascript.h"
#include "js/quickjs.h"
#include "js/quickjs/document.h"
#include "js/quickjs/heartbeat.h"
#include "js/quickjs/image.h"
#include "js/quickjs/node.h"
#include "js/timer.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "main/timer.h"
#include "network/connection.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "session/download.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/qs_parse/qs_parse.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_image_class_id;

static JSValue
js_image_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	JSValue document_obj = interpreter->document_obj;
	dom_html_document *doc = (struct dom_html_document *)js_doc_getopaque(document_obj);

	if (!doc) {
		return JS_NULL;
	}

	dom_element *element = NULL;
	dom_exception exc = dom_document_create_element(doc, corestring_dom_IMG, &element);

	if (exc != DOM_NO_ERR || !element) {
		//dom_node_unref(doc);
		return JS_NULL;
	}

	if (argc > 0) {
		size_t width_len;
		const char *width = JS_ToCStringLen(ctx, &width_len, argv[0]);

		if (width) {
			dom_string *value_str = NULL;
			exc = dom_string_create((const uint8_t *)width, width_len, &value_str);
			JS_FreeCString(ctx, width);

			if (exc == DOM_NO_ERR) {
				(void)dom_element_set_attribute(element, corestring_dom_width, value_str);
				dom_string_unref(value_str);
			}
		}
	}

	if (argc > 1) {
		size_t height_len;
		const char *height = JS_ToCStringLen(ctx, &height_len, argv[1]);

		if (height) {
			dom_string *value_str = NULL;
			exc = dom_string_create((const uint8_t *)height, height_len, &value_str);
			JS_FreeCString(ctx, height);

			if (exc == DOM_NO_ERR) {
				(void)dom_element_set_attribute(element, corestring_dom_height, value_str);
				dom_string_unref(value_str);
			}
		}
	}
	//dom_node_unref(doc);
	JSValue rr = getNode(ctx, element);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(element);

	return rr;
}

static void
JS_NewGlobalCConstructor2(JSContext *ctx, JSValue func_obj, const char *name, JSValueConst proto)
{
	ELOG
	REF_JS(func_obj);
	REF_JS(proto);

	JSValue global_object = JS_GetGlobalObject(ctx);

	JS_DefinePropertyValueStr(ctx, global_object, name,
		JS_DupValue(ctx, func_obj), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
	JS_SetConstructor(ctx, func_obj, proto);
	JS_FreeValue(ctx, func_obj);
	JS_FreeValue(ctx, global_object);
}

static JSValueConst
JS_NewGlobalCConstructor(JSContext *ctx, const char *name, JSCFunction *func, int length, JSValueConst proto)
{
	ELOG
	JSValue func_obj;
	func_obj = JS_NewCFunction2(ctx, func, name, length, JS_CFUNC_constructor_or_func, 0);
	REF_JS(func_obj);
	REF_JS(proto);

	JS_NewGlobalCConstructor2(ctx, func_obj, name, proto);

	return func_obj;
}

static JSClassDef js_image_class = {
	"Image",
};

static const JSCFunctionListEntry js_image_proto_funcs[] = {
};

int
js_image_init(JSContext *ctx)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue proto;

	/* urlSearchParams class */
	JS_NewClassID(&js_image_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_image_class_id, &js_image_class);
	proto = JS_NewObject(ctx);
	REF_JS(proto);

	JS_SetPropertyFunctionList(ctx, proto, js_image_proto_funcs, countof(js_image_proto_funcs));
	JS_SetClassProto(ctx, js_image_class_id, proto);

	/* Image object */
	(void)JS_NewGlobalCConstructor(ctx, "Image", js_image_constructor, 1, proto);
	//REF_JS(obj);

	return 0;
}
