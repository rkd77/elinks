/* ECMAScript browser scripting module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "ecmascript/spidermonkey-shared.h"
#include "network/connection.h"
#include "protocol/uri.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/cache_object.h"
#include "scripting/smjs/elinks_object.h"
#include "session/download.h"


struct smjs_load_uri_hop {
	struct session *ses;

	/* SpiderMonkey versions earlier than 1.8 cannot properly call
	 * a closure if given just a JSFunction pointer.  They need a
	 * jsval that points to the corresponding JSObject.  Besides,
	 * JS_AddNamedRoot is not documented to support JSFunction
	 * pointers.  */
	jsval callback;
};

static void
smjs_loading_callback(struct download *download, void *data)
{
	struct session *saved_smjs_ses = smjs_ses;
	struct smjs_load_uri_hop *hop = data;
	jsval args[1], rval;
	JSObject *cache_entry_object;

	if (is_in_progress_state(download->state)) return;

	if (!download->cached) goto end;

	/* download->cached->object.refcount is typically 0 here
	 * because no struct document uses the cache entry.  Because
	 * the connection is no longer using the cache entry either,
	 * it can be garbage collected.  Don't let that happen while
	 * the script is using it.  */
	object_lock(download->cached);

	smjs_ses = hop->ses;

	cache_entry_object = smjs_get_cache_entry_object(download->cached);
	if (!cache_entry_object) goto end;

	args[0] = OBJECT_TO_JSVAL(cache_entry_object);
	JS_CallFunctionValue(smjs_ctx, NULL, hop->callback, 1, args, &rval);

end:
	if (download->cached)
		object_unlock(download->cached);
	JS_RemoveRoot(smjs_ctx, &hop->callback);
	mem_free(download->data);
	mem_free(download);

	smjs_ses = saved_smjs_ses;
}

static JSBool
smjs_load_uri(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval)
{
	struct smjs_load_uri_hop *hop;
	struct download *download;
	JSString *jsstr;
	unsigned char *uri_string;
	struct uri *uri;

	if (argc < 2) return JS_FALSE;

	jsstr = JS_ValueToString(smjs_ctx, argv[0]);
	uri_string = JS_GetStringBytes(jsstr);

	uri = get_uri(uri_string, 0);
	if (!uri) return JS_FALSE;

	download = mem_alloc(sizeof(*download));
	if (!download) {
		done_uri(uri);
		return JS_FALSE;
	}

	hop = mem_alloc(sizeof(*hop));
	if (!hop) {
		mem_free(download);
		done_uri(uri);
		return JS_FALSE;
	}

	hop->callback = argv[1];
	hop->ses = smjs_ses;
	if (!JS_AddNamedRoot(smjs_ctx, &hop->callback,
			     "smjs_load_uri_hop.callback")) {
		mem_free(hop);
		mem_free(download);
		done_uri(uri);
		return JS_FALSE;
	}

	download->data = hop;
	download->callback = (download_callback_T *) smjs_loading_callback;

	load_uri(uri, NULL, download, PRI_MAIN, CACHE_MODE_NORMAL, -1);

	done_uri(uri);

	return JS_TRUE;
}

void
smjs_init_load_uri_interface(void)
{
	if (!smjs_ctx || !smjs_elinks_object)
		return;

	JS_DefineFunction(smjs_ctx, smjs_elinks_object, "load_uri",
	                  &smjs_load_uri, 2, 0);
}
