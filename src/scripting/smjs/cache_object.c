/* Exports struct cache_entry to the world of ECMAScript */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "ecmascript/spidermonkey/util.h"
#include "protocol/uri.h"
#include "scripting/smjs/cache_object.h"
#include "scripting/smjs/core.h"
#include "util/error.h"
#include "util/memory.h"

static const JSClass cache_entry_class; /* defined below */

enum cache_entry_prop {
	CACHE_ENTRY_CONTENT,
	CACHE_ENTRY_TYPE,
	CACHE_ENTRY_LENGTH,
	CACHE_ENTRY_HEAD,
	CACHE_ENTRY_URI,
};

static const JSPropertySpec cache_entry_props[] = {
	{ "content", CACHE_ENTRY_CONTENT, JSPROP_ENUMERATE },
	{ "type",    CACHE_ENTRY_TYPE,    JSPROP_ENUMERATE },
	{ "length",  CACHE_ENTRY_LENGTH,  JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "head",    CACHE_ENTRY_HEAD,    JSPROP_ENUMERATE },
	{ "uri",     CACHE_ENTRY_URI,     JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

/* @cache_entry_class.getProperty */
static JSBool
cache_entry_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct cache_entry *cached;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &cache_entry_class, NULL))
		return JS_FALSE;

	cached = JS_GetInstancePrivate(ctx, obj, 
				       (JSClass *) &cache_entry_class, NULL);

	if (!cache_entry_is_valid(cached)) return JS_FALSE;

	undef_to_jsval(ctx, vp);

	if (!JSVAL_IS_INT(id))
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case CACHE_ENTRY_CONTENT: {
		struct fragment *fragment = get_cache_fragment(cached);

		assert(fragment);

		*vp = STRING_TO_JSVAL(JS_NewStringCopyN(smjs_ctx,
	                                                fragment->data,
	                                                fragment->length));

		return JS_TRUE;
	}
	case CACHE_ENTRY_TYPE:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx,
	                                                cached->content_type));

		return JS_TRUE;
	case CACHE_ENTRY_HEAD:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx,
	                                                cached->head));

		return JS_TRUE;
	case CACHE_ENTRY_LENGTH:
		*vp = INT_TO_JSVAL(cached->length);

		return JS_TRUE;
	case CACHE_ENTRY_URI:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx,
		                                        struri(cached->uri)));

		return JS_TRUE;
	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return JS_TRUE in this case
		 * and leave *@vp unchanged.  Do the same here.
		 * (Actually not quite the same, as we already used
		 * @undef_to_jsval.)  */
		return JS_TRUE;
	}
}

/* @cache_entry_class.setProperty */
static JSBool
cache_entry_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct cache_entry *cached;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &cache_entry_class, NULL))
		return JS_FALSE;

	cached = JS_GetInstancePrivate(ctx, obj,
				       (JSClass *) &cache_entry_class, NULL);

	if (!cache_entry_is_valid(cached)) return JS_FALSE;

	if (!JSVAL_IS_INT(id))
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case CACHE_ENTRY_CONTENT: {
		JSString *jsstr = JS_ValueToString(smjs_ctx, *vp);
		unsigned char *str = JS_GetStringBytes(jsstr);
		size_t len = JS_GetStringLength(jsstr);

		add_fragment(cached, 0, str, len);
		normalize_cache_entry(cached, len);

		return JS_TRUE;
	}
	case CACHE_ENTRY_TYPE: {
		JSString *jsstr = JS_ValueToString(smjs_ctx, *vp);
		unsigned char *str = JS_GetStringBytes(jsstr);

		mem_free_set(&cached->content_type, stracpy(str));

		return JS_TRUE;
	}
	case CACHE_ENTRY_HEAD: {
		JSString *jsstr = JS_ValueToString(smjs_ctx, *vp);
		unsigned char *str = JS_GetStringBytes(jsstr);

		mem_free_set(&cached->head, stracpy(str));

		return JS_TRUE;
	}
	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return JS_TRUE in this case.
		 * Do the same here.  */
		return JS_TRUE;
	}
}

/* @cache_entry_class.finalize */
static void
cache_entry_finalize(JSContext *ctx, JSObject *obj)
{
	struct cache_entry *cached;

	assert(JS_InstanceOf(ctx, obj, (JSClass *) &cache_entry_class, NULL));
	if_assert_failed return;

	cached = JS_GetInstancePrivate(ctx, obj,
				       (JSClass *) &cache_entry_class, NULL);

	if (!cached) return;

	object_unlock(cached);
}
	
static const JSClass cache_entry_class = {
	"cache_entry",
	JSCLASS_HAS_PRIVATE,	/* struct cache_entry * */
	JS_PropertyStub, JS_PropertyStub,
	cache_entry_get_property, cache_entry_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, cache_entry_finalize
};

JSObject *
smjs_get_cache_entry_object(struct cache_entry *cached)
{
	JSObject *cache_entry_object;
		
	assert(smjs_ctx);

	cache_entry_object = JS_NewObject(smjs_ctx,
	                                  (JSClass *) &cache_entry_class,
	                                  NULL, NULL);

	if (!cache_entry_object) return NULL;

	if (JS_FALSE == JS_SetPrivate(smjs_ctx, cache_entry_object, cached)) /* to @cache_entry_class */
		return NULL;

	object_lock(cached);

	if (JS_FALSE == JS_DefineProperties(smjs_ctx, cache_entry_object,
	                               (JSPropertySpec *) cache_entry_props))
		return NULL;

	return cache_entry_object;
}
