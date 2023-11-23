/* The quickjs localstorage object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/leds.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/localstorage-db.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/localstorage.h"
#include "session/session.h"
#include "viewer/text/vs.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_localstorage_class_id;

/* IMPLEMENTS READ FROM STORAGE USING SQLITE DATABASE */
static char *
readFromStorage(const char *key)
{
	char *val;

	if (local_storage_ready==0)
	{
		db_prepare_structure(local_storage_filename);
		local_storage_ready=1;
	}

	val = db_query_by_key(local_storage_filename, key);

	//DBG("Read: %s %s %s",local_storage_filename, key, val);

	return val;
}

static void
removeFromStorage(const char *key)
{
	if (local_storage_ready==0)
	{
		db_prepare_structure(local_storage_filename);
		local_storage_ready=1;
	}
	db_delete_from(local_storage_filename, key);
}

/* IMPLEMENTS SAVE TO STORAGE USING SQLITE DATABASE */
static void
saveToStorage(const char *key, const char *val)
{
	if (local_storage_ready==0) {
		db_prepare_structure(local_storage_filename);
		local_storage_ready=1;
	}

	int rows_affected=0;

	rows_affected=db_update_set(local_storage_filename, key, val);

	if (rows_affected==0) {
		rows_affected=db_insert_into(local_storage_filename, key, val);
	}

	// DBG(log, "UPD ROWS: %d KEY: %s VAL: %s",rows_affected,key,val);

}

static JSValue
js_localstorage_getitem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1)
	{
		return JS_UNDEFINED;
	}

	const char *key;
	size_t len;

	key = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!key) {
		return JS_EXCEPTION;
	}

	char *val = readFromStorage(key);
	JS_FreeCString(ctx, key);

	if (!val) {
		return JS_NULL;
	}

	JSValue ret = JS_NewString(ctx, val);
	mem_free(val);
	RETURN_JS(ret);
}

static JSValue
js_localstorage_removeitem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1)
	{
		return JS_UNDEFINED;
	}

	const char *key;
	size_t len;

	key = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!key) {
		return JS_EXCEPTION;
	}

	removeFromStorage(key);
	JS_FreeCString(ctx, key);

	return JS_UNDEFINED;
}

/* @localstorage_funcs{"setItem"} */
static JSValue
js_localstorage_setitem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);

	if (argc != 2)
	{
		return JS_UNDEFINED;
	}

	const char *key_str, *val_str;
	size_t len_key, len_val;

	key_str = JS_ToCStringLen(ctx, &len_key, argv[0]);

	if (!key_str) {
		return JS_EXCEPTION;
	}

	val_str = JS_ToCStringLen(ctx, &len_val, argv[1]);

	if (!val_str) {
		JS_FreeCString(ctx, key_str);
		return JS_EXCEPTION;
	}

	saveToStorage(key_str, val_str);
	JS_FreeCString(ctx, key_str);
	JS_FreeCString(ctx, val_str);

#ifdef CONFIG_LEDS
	set_led_value(interpreter->vs->doc_view->session->status.ecmascript_led, 'J');
#endif
	return JS_TRUE;
}

static JSValue
js_localstorage_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[localstorage object]");
}

static const JSCFunctionListEntry js_localstorage_proto_funcs[] = {
	JS_CFUNC_DEF("getItem", 1, js_localstorage_getitem),
	JS_CFUNC_DEF("removeItem", 1, js_localstorage_removeitem),
	JS_CFUNC_DEF("setItem", 2, js_localstorage_setitem),
	JS_CFUNC_DEF("toString", 0, js_localstorage_toString)
};

static JSClassDef js_localstorage_class = {
	"localStorage",
};

int
js_localstorage_init(JSContext *ctx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	static int initialized;

	if (!initialized) {
		/* create the localstorage class */
		JS_NewClassID(&js_localstorage_class_id);
		JS_NewClass(JS_GetRuntime(ctx), js_localstorage_class_id, &js_localstorage_class);
		initialized = 1;
	}

	JSValue global_obj = JS_GetGlobalObject(ctx);
	REF_JS(global_obj);

	JSValue localstorage_obj = JS_NewObjectClass(ctx, js_localstorage_class_id);
	REF_JS(localstorage_obj);

	JS_SetPropertyFunctionList(ctx, localstorage_obj, js_localstorage_proto_funcs, countof(js_localstorage_proto_funcs));
	REF_JS(localstorage_obj);

	JS_SetClassProto(ctx, js_localstorage_class_id, localstorage_obj);
	REF_JS(localstorage_obj);

	JS_SetPropertyStr(ctx, global_obj, "localStorage", JS_DupValue(ctx, localstorage_obj));
	REF_JS(localstorage_obj);

	JS_FreeValue(ctx, global_obj);

	return 0;
}
