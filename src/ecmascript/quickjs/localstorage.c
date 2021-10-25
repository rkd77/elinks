/* The quickjs localstorage object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "config/home.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/localstorage-db.h"
#include "ecmascript/quickjs/localstorage.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"

#include <time.h>
#include "document/renderer.h"
#include "document/refresh.h"
#include "terminal/screen.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_localstorage_class_id;

/* IMPLEMENTS READ FROM STORAGE USING SQLITE DATABASE */
static unsigned char *
readFromStorage(const unsigned char *key)
{

	char * val;

	if (local_storage_ready==0)
	{
		db_prepare_structure(local_storage_filename);
		local_storage_ready=1;
	}

	val = db_query_by_key(local_storage_filename, key);

	//DBG("Read: %s %s %s",local_storage_filename, key, val);

	return (val);
}

/* IMPLEMENTS SAVE TO STORAGE USING SQLITE DATABASE */
static void
saveToStorage(const unsigned char *key, const unsigned char *val)
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);

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

	if (key) {
		unsigned char *val = readFromStorage(key);

		JSValue ret = JS_NewString(ctx, val);

		mem_free(val);
		JS_FreeCString(ctx, key);
		return ret;
	}

	return JS_UNDEFINED;
}

/* @localstorage_funcs{"setItem"} */
static JSValue
js_localstorage_setitem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

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

static const JSCFunctionListEntry js_localstorage_proto_funcs[] = {
	JS_CFUNC_DEF("setItem", 2, js_localstorage_setitem),
	JS_CFUNC_DEF("getItem", 1, js_localstorage_getitem),
};

static JSClassDef js_localstorage_class = {
	"localStorage",
};

static JSValue
js_localstorage_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_localstorage_class_id);
	JS_FreeValue(ctx, proto);

	if (JS_IsException(obj)) {
		goto fail;
	}
	return obj;

fail:
	JS_FreeValue(ctx, obj);
	return JS_EXCEPTION;
}

int
js_localstorage_init(JSContext *ctx, JSValue global_obj)
{
	JSValue localstorage_proto, localstorage_class;

	/* create the localstorage class */
	JS_NewClassID(&js_localstorage_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_localstorage_class_id, &js_localstorage_class);

	localstorage_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, localstorage_proto, js_localstorage_proto_funcs, countof(js_localstorage_proto_funcs));

	localstorage_class = JS_NewCFunction2(ctx, js_localstorage_ctor, "localStorage", 0, JS_CFUNC_constructor, 0);
	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, localstorage_class, localstorage_proto);
	JS_SetClassProto(ctx, js_localstorage_class_id, localstorage_proto);

	JS_SetPropertyStr(ctx, global_obj, "localStorage", localstorage_proto);
	return 0;
}
