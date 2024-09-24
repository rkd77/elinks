/* The QuickJS URLSearchParams object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "js/ecmascript.h"
#include "js/quickjs.h"
#include "js/quickjs/heartbeat.h"
#include "js/quickjs/urlsearchparams.h"
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

static JSClassID js_urlSearchParams_class_id;

struct eljs_urlSearchParams {
	JSValue map;
};

static struct string result;
static char *prepend;

static JSValue map_foreach_callback(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

static
void js_urlSearchParams_finalizer(JSRuntime *rt, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);

	struct eljs_urlSearchParams *u = (struct eljs_urlSearchParams *)JS_GetOpaque(val, js_urlSearchParams_class_id);

	if (u) {
		JS_FreeValueRT(rt, u->map);
		mem_free(u);
	}
}

static void
js_urlSearchParams_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);

	struct eljs_urlSearchParams *u = (struct eljs_urlSearchParams *)JS_GetOpaque(val, js_urlSearchParams_class_id);

	if (u) {
		JS_MarkValue(rt, u->map, mark_func);
	}
}

static void
parse_text(JSContext *ctx, JSValue map, char *str)
{
	if (!str || !*str) {
		return;
	}

	char *kvpairs[1024];
	int i = qs_parse(str, kvpairs, 1024);
	int j;

	for (j = 0; j < i; j++) {
		char *key = kvpairs[j];
		char *value = strchr(key, '=');
		if (value) {
			*value++ = '\0';
		}
		JSValue argv[2];

		argv[0] = JS_NewString(ctx, key);
		argv[1] = JS_NewString(ctx, value);
		JSValue set = JS_GetPropertyStr(ctx, map, "set");
		(void)JS_Call(ctx, set, map, 2, argv);
		JS_FreeValue(ctx, set);
		JS_FreeValue(ctx, argv[0]);
		JS_FreeValue(ctx, argv[1]);
		JS_FreeValue(ctx, map);
	}
}

static JSValue
js_urlSearchParams_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(new_target);

	JSValue obj = JS_NewObjectClass(ctx, js_urlSearchParams_class_id);
	REF_JS(obj);

	if (JS_IsException(obj)) {
		return obj;
	}
	struct eljs_urlSearchParams *u = (struct eljs_urlSearchParams *)mem_calloc(1, sizeof(*u));

	if (!u) {
		JS_FreeValue(ctx, obj);
		return JS_EXCEPTION;
	}

	if (argc > 0) {
		if (JS_IsArray(ctx, argv[0])) {
			u->map = JS_Eval(ctx, "new Map();", strlen("new Map();"), "", JS_EVAL_TYPE_GLOBAL);
			JSValue l = JS_GetPropertyStr(ctx, argv[0], "length");

			uint32_t len = 0;
			JS_ToUint32(ctx, &len, l);
			JS_FreeValue(ctx, l);

			for (int i = 0; i < len; i++) {
				JSValue val = JS_GetPropertyUint32(ctx, argv[0], i);
				JSValue l2 = JS_GetPropertyStr(ctx, val, "length");

				uint32_t len2 = 0;
				JS_ToUint32(ctx, &len2, l2);
				JS_FreeValue(ctx, l2);

				if (len2 != 2) {
					JS_FreeValue(ctx, val);
					continue;
				}
				JSValue argv[2];
				argv[0] = JS_GetPropertyUint32(ctx, val, 0);
				argv[1] = JS_GetPropertyUint32(ctx, val, 1);
				JS_FreeValue(ctx, val);

				JSValue set = JS_GetPropertyStr(ctx, u->map, "set");
				(void)JS_Call(ctx, set, u->map, 2, argv);
				JS_FreeValue(ctx, set);
				JS_FreeValue(ctx, argv[0]);
				JS_FreeValue(ctx, argv[1]);
				JS_FreeValue(ctx, u->map);
			}
		} else if (JS_IsObject(argv[0])) {
			u->map = JS_Eval(ctx, "new Map();", strlen("new Map();"), "", JS_EVAL_TYPE_GLOBAL);

			JSPropertyEnum *ptab = NULL;
			uint32_t plen = 0;
			JS_GetOwnPropertyNames(ctx, &ptab, &plen, argv[0], JS_GPN_STRING_MASK);

			for (int i = 0; i < plen; i++) {
				JSValue vv[2];
				vv[0] = JS_AtomToValue(ctx, ptab[i].atom);
				vv[1] = JS_GetProperty(ctx, argv[0], ptab[i].atom);
				JSValue set = JS_GetPropertyStr(ctx, u->map, "set");
				(void)JS_Call(ctx, set, u->map, 2, vv);
				JS_FreeValue(ctx, set);
				JS_FreeValue(ctx, vv[0]);
				JS_FreeValue(ctx, vv[1]);
				JS_FreeValue(ctx, u->map);
			}
			if (ptab) {
				free(ptab);
			}
		} else {
			size_t len;
			const char *str = JS_ToCStringLen(ctx, &len, argv[0]);

			if (!str) {
				JS_FreeValue(ctx, obj);
				return JS_EXCEPTION;
			}
			char *urlstring = memacpy(str, len);
			JS_FreeCString(ctx, str);

			if (!urlstring) {
				return JS_EXCEPTION;
			}
			u->map = JS_Eval(ctx, "new Map();", strlen("new Map();"), "", JS_EVAL_TYPE_GLOBAL);
			parse_text(ctx, u->map, urlstring);
			mem_free(urlstring);
		}
	}
	JS_SetOpaque(obj, u);

	return obj;
}

static void
JS_NewGlobalCConstructor2(JSContext *ctx, JSValue func_obj, const char *name, JSValueConst proto)
{
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
	JSValue func_obj;
	func_obj = JS_NewCFunction2(ctx, func, name, length, JS_CFUNC_constructor_or_func, 0);
	REF_JS(func_obj);
	REF_JS(proto);

	JS_NewGlobalCConstructor2(ctx, func_obj, name, proto);

	return func_obj;
}


static JSValue
js_urlSearchParams_get_property_size(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_urlSearchParams *u = (struct eljs_urlSearchParams *)(JS_GetOpaque(this_val, js_urlSearchParams_class_id));

	if (!u) {
		return JS_NULL;
	}
	JSValue ret = JS_GetPropertyStr(ctx, u->map, "size");

	RETURN_JS(ret);
}


static JSValue
js_urlSearchParams_delete(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct eljs_urlSearchParams *u = (struct eljs_urlSearchParams *)(JS_GetOpaque(this_val, js_urlSearchParams_class_id));

	if (!u) {
		return JS_NULL;
	}
	JSValue del = JS_GetPropertyStr(ctx, u->map, "delete");
	JS_Call(ctx, del, u->map, argc, argv);
	JS_FreeValue(ctx, del);

	return JS_UNDEFINED;
}

static JSValue
js_urlSearchParams_entries(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct eljs_urlSearchParams *u = (struct eljs_urlSearchParams *)(JS_GetOpaque(this_val, js_urlSearchParams_class_id));

	if (!u) {
		return JS_NULL;
	}
	JSValue entries = JS_GetPropertyStr(ctx, u->map, "entries");

	RETURN_JS(entries);
}

static JSValue
js_urlSearchParams_forEach(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct eljs_urlSearchParams *u = (struct eljs_urlSearchParams *)(JS_GetOpaque(this_val, js_urlSearchParams_class_id));

	if (!u) {
		return JS_NULL;
	}

	if (argc < 1) {
		return JS_UNDEFINED;
	}
	JSValue forEach = JS_GetPropertyStr(ctx, u->map, "forEach");
	JS_Call(ctx, forEach, u->map, argc, argv);
	JS_FreeValue(ctx, forEach);

	return JS_UNDEFINED;
}

static JSValue
js_urlSearchParams_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct eljs_urlSearchParams *u = (struct eljs_urlSearchParams *)(JS_GetOpaque(this_val, js_urlSearchParams_class_id));

	if (!u) {
		return JS_NULL;
	}

	if (argc == 1) {
		JSValue get = JS_GetPropertyStr(ctx, u->map, "get");
		JSValue ret = JS_Call(ctx, get, u->map, argc, argv);
		JS_FreeValue(ctx, get);

		RETURN_JS(ret);
	}

	return JS_UNDEFINED;
}

static JSValue
js_urlSearchParams_has(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct eljs_urlSearchParams *u = (struct eljs_urlSearchParams *)(JS_GetOpaque(this_val, js_urlSearchParams_class_id));

	if (!u) {
		return JS_NULL;
	}

	if (argc == 1) {
		JSValue has = JS_GetPropertyStr(ctx, u->map, "has");
		JSValue ret = JS_Call(ctx, has, u->map, argc, argv);
		JS_FreeValue(ctx, has);

		RETURN_JS(ret);
	}

	if (argc > 1) {
		JSValue get = JS_GetPropertyStr(ctx, u->map, "get");
		JSValue ret = JS_Call(ctx, get, u->map, argc, argv);
		JS_FreeValue(ctx, get);

		const char *ret1 = JS_ToCString(ctx, ret);
		const char *val = JS_ToCString(ctx, argv[1]);

		JS_FreeValue(ctx, ret);

		if (ret1 && val) {
			bool r = !strcmp(ret1, val);

			JS_FreeCString(ctx, ret1);
			JS_FreeCString(ctx, val);

			return JS_NewBool(ctx, r);
		}
	}

	return JS_FALSE;
}

static JSValue
js_urlSearchParams_keys(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct eljs_urlSearchParams *u = (struct eljs_urlSearchParams *)(JS_GetOpaque(this_val, js_urlSearchParams_class_id));

	if (!u) {
		return JS_NULL;
	}
	JSValue keys = JS_GetPropertyStr(ctx, u->map, "keys");

	RETURN_JS(keys);
}

static JSValue
js_urlSearchParams_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct eljs_urlSearchParams *u = (struct eljs_urlSearchParams *)(JS_GetOpaque(this_val, js_urlSearchParams_class_id));

	if (!u) {
		return JS_NULL;
	}

	if (argc < 1) {
		return JS_UNDEFINED;
	}
	JSValue set = JS_GetPropertyStr(ctx, u->map, "set");
	JS_Call(ctx, set, u->map, argc, argv);
	JS_FreeValue(ctx, set);

	return JS_UNDEFINED;
}

static JSValue
map_foreach_callback(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *val = JS_ToCString(ctx, argv[0]);
	const char *key = JS_ToCString(ctx, argv[1]);
	add_to_string(&result, prepend);

	if (key) {
		add_to_string(&result, key);
		add_char_to_string(&result, '=');
		JS_FreeCString(ctx, key);
	}

	if (val) {
		add_to_string(&result, val);
		JS_FreeCString(ctx, val);
	}
	prepend = "&";

	return JS_UNDEFINED;
}

static JSValue
js_urlSearchParams_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct eljs_urlSearchParams *u = (struct eljs_urlSearchParams *)(JS_GetOpaque(this_val, js_urlSearchParams_class_id));

	if (!u) {
		return JS_NULL;
	}

//	return JS_NewString(ctx, "aaa");

	if (!init_string(&result)) {
		return JS_EXCEPTION;
	}
	prepend = "";
	JSValue fun = JS_NewCFunction(ctx, map_foreach_callback, "f", 3);
	JSValue forEach = JS_GetPropertyStr(ctx, u->map, "forEach");
	JS_Call(ctx, forEach, u->map, 2, &fun);
	JS_FreeValue(ctx, forEach);
	JS_FreeValue(ctx, fun);

	JSValue ret = JS_NewStringLen(ctx, result.source, result.length);
	done_string(&result);

	RETURN_JS(ret);
}

static JSValue
js_urlSearchParams_values(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct eljs_urlSearchParams *u = (struct eljs_urlSearchParams *)(JS_GetOpaque(this_val, js_urlSearchParams_class_id));

	if (!u) {
		return JS_NULL;
	}
	JSValue values = JS_GetPropertyStr(ctx, u->map, "values");

	RETURN_JS(values);
}

static JSClassDef js_urlSearchParams_class = {
	"URLSearchParams",
	.finalizer = js_urlSearchParams_finalizer,
	.gc_mark = js_urlSearchParams_mark
};

static const JSCFunctionListEntry js_urlSearchParams_proto_funcs[] = {
	JS_CGETSET_DEF("size",	js_urlSearchParams_get_property_size, NULL),
	JS_CFUNC_DEF("delete",			1, js_urlSearchParams_delete ),
	JS_CFUNC_DEF("entries",		0, js_urlSearchParams_entries ),
	JS_CFUNC_DEF("forEach",		2, js_urlSearchParams_forEach ),
	JS_CFUNC_DEF("get",			1, js_urlSearchParams_get ),
	JS_CFUNC_DEF("has",			2, js_urlSearchParams_has ),
	JS_CFUNC_DEF("keys",			0, js_urlSearchParams_keys ),
	JS_CFUNC_DEF("set",			2, js_urlSearchParams_set ),
	JS_CFUNC_DEF("toString",		0, js_urlSearchParams_toString ),
	JS_CFUNC_DEF("values",			0, js_urlSearchParams_values ),
};

int
js_urlSearchParams_init(JSContext *ctx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue proto;

	/* urlSearchParams class */
	JS_NewClassID(&js_urlSearchParams_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_urlSearchParams_class_id, &js_urlSearchParams_class);
	proto = JS_NewObject(ctx);
	REF_JS(proto);

	JS_SetPropertyFunctionList(ctx, proto, js_urlSearchParams_proto_funcs, countof(js_urlSearchParams_proto_funcs));
	JS_SetClassProto(ctx, js_urlSearchParams_class_id, proto);

	/* url object */
	(void)JS_NewGlobalCConstructor(ctx, "URLSearchParams", js_urlSearchParams_constructor, 1, proto);
	//REF_JS(obj);

	return 0;
}
