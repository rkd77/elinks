/* The QuickJS dom tokenlist object implementation. */

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
#include "ecmascript/ecmascript.h"
#include "ecmascript/quickjs/mapa.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/element.h"
#include "ecmascript/quickjs/tokenlist.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

JSClassID js_tokenlist_class_id;

static
void js_tokenlist_finalizer(JSRuntime *rt, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);
	dom_tokenlist *tl = (dom_tokenlist *)(JS_GetOpaque(val, js_tokenlist_class_id));

	if (tl) {
		dom_tokenlist_unref(tl);
	}
}

static JSValue
js_tokenlist_add(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	dom_tokenlist *tl = (dom_tokenlist *)(JS_GetOpaque(this_val, js_tokenlist_class_id));

	if (!tl || argc < 1) {
		return JS_UNDEFINED;
	}
	size_t size;
	const char *klass = JS_ToCStringLen(ctx, &size, argv[0]);

	if (!klass) {
		return JS_UNDEFINED;
	}
	dom_string *kl = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)klass, size, &kl);
	JS_FreeCString(ctx, klass);

	if (exc != DOM_NO_ERR || !kl) {
		return JS_UNDEFINED;
	}
	exc = dom_tokenlist_add(tl, kl);
	dom_string_unref(kl);

	if (exc == DOM_NO_ERR) {
		interpreter->changed = true;
	}

	return JS_UNDEFINED;
}

static JSValue
js_tokenlist_contains(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	dom_tokenlist *tl = (dom_tokenlist *)(JS_GetOpaque(this_val, js_tokenlist_class_id));

	if (!tl || argc < 1) {
		return JS_UNDEFINED;
	}
	size_t size;
	const char *klass = JS_ToCStringLen(ctx, &size, argv[0]);

	if (!klass) {
		return JS_UNDEFINED;
	}
	dom_string *kl = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)klass, size, &kl);
	JS_FreeCString(ctx, klass);

	if (exc != DOM_NO_ERR || !kl) {
		return JS_UNDEFINED;
	}
	bool res = false;
	exc = dom_tokenlist_contains(tl, kl, &res);
	dom_string_unref(kl);

	if (exc == DOM_NO_ERR) {
		return JS_NewBool(ctx, res);
	}

	return JS_UNDEFINED;
}

static JSValue
js_tokenlist_remove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	dom_tokenlist *tl = (dom_tokenlist *)(JS_GetOpaque(this_val, js_tokenlist_class_id));

	if (!tl || argc < 1) {
		return JS_UNDEFINED;
	}
	size_t size;
	const char *klass = JS_ToCStringLen(ctx, &size, argv[0]);

	if (!klass) {
		return JS_UNDEFINED;
	}

	dom_string *kl = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)klass, size, &kl);
	JS_FreeCString(ctx, klass);

	if (exc != DOM_NO_ERR || !kl) {
		return JS_UNDEFINED;
	}
	exc = dom_tokenlist_remove(tl, kl);
	dom_string_unref(kl);

	if (exc == DOM_NO_ERR) {
		interpreter->changed = true;
	}

	return JS_UNDEFINED;
}

static JSValue
js_tokenlist_toggle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	dom_tokenlist *tl = (dom_tokenlist *)(JS_GetOpaque(this_val, js_tokenlist_class_id));

	if (!tl || argc < 1) {
		return JS_UNDEFINED;
	}
	size_t size;
	const char *klass = JS_ToCStringLen(ctx, &size, argv[0]);

	if (!klass) {
		return JS_UNDEFINED;
	}
	dom_string *kl = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)klass, size, &kl);
	JS_FreeCString(ctx, klass);

	if (exc != DOM_NO_ERR || !kl) {
		return JS_UNDEFINED;
	}
	bool res = false;
	exc = dom_tokenlist_contains(tl, kl, &res);

	if (exc == DOM_NO_ERR) {
		if (res) {
			exc = dom_tokenlist_remove(tl, kl);
		} else {
			exc = dom_tokenlist_add(tl, kl);
		}
		if (exc == DOM_NO_ERR) {
			interpreter->changed = true;
		}
	}
	dom_string_unref(kl);

	return JS_UNDEFINED;
}

static JSValue
js_tokenlist_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[tokenlist object]");
}

static const JSCFunctionListEntry js_tokenlist_proto_funcs[] = {
	JS_CFUNC_DEF("add", 1, js_tokenlist_add),
	JS_CFUNC_DEF("contains", 1, js_tokenlist_contains),
	JS_CFUNC_DEF("remove", 1, js_tokenlist_remove),
	JS_CFUNC_DEF("toggle", 1, js_tokenlist_toggle),
	JS_CFUNC_DEF("toString", 0, js_tokenlist_toString)
};

static JSClassDef js_tokenlist_class = {
	"tokenlist",
	.finalizer = js_tokenlist_finalizer
};

JSValue
getTokenlist(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	/* nodelist class */
	static int initialized;

	if (!initialized) {
		JS_NewClassID(&js_tokenlist_class_id);
		JS_NewClass(JS_GetRuntime(ctx), js_tokenlist_class_id, &js_tokenlist_class);
		initialized = 1;
	}
	JSValue proto = JS_NewObjectClass(ctx, js_tokenlist_class_id);
	REF_JS(proto);

	JS_SetPropertyFunctionList(ctx, proto, js_tokenlist_proto_funcs, countof(js_tokenlist_proto_funcs));
	JS_SetClassProto(ctx, js_tokenlist_class_id, proto);
	JS_SetOpaque(proto, node);
	JSValue rr = JS_DupValue(ctx, proto);

	RETURN_JS(rr);
}
