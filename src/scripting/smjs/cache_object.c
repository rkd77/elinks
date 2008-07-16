/* Exports struct cache_entry to the world of ECMAScript */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "ecmascript/spidermonkey-shared.h"
#include "protocol/uri.h"
#include "scripting/smjs/cache_object.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/smjs.h"
#include "util/error.h"
#include "util/memory.h"

static const JSClass cache_entry_class; /* defined below */

/* Tinyids of properties.  Use negative values to distinguish these
 * from array indexes (even though this object has no array elements).
 * ECMAScript code should not use these directly as in cache_entry[-1];
 * future versions of ELinks may change the numbers.  */
enum cache_entry_prop {
	CACHE_ENTRY_CONTENT = -1,
	CACHE_ENTRY_TYPE    = -2,
	CACHE_ENTRY_LENGTH  = -3,
	CACHE_ENTRY_HEAD    = -4,
	CACHE_ENTRY_URI     = -5,
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
	JSBool ret;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &cache_entry_class, NULL))
		return JS_FALSE;

	cached = JS_GetInstancePrivate(ctx, obj,
				       (JSClass *) &cache_entry_class, NULL);
	if (!cached) return JS_FALSE; /* already detached */

	assert(cache_entry_is_valid(cached));
	if_assert_failed return JS_FALSE;

	/* Get a strong reference to the cache entry to prevent it
	 * from being deleted if some function called below decides to
	 * collect garbage.  After this, all code paths must
	 * eventually unlock the object.  */
	object_lock(cached);

	undef_to_jsval(ctx, vp);

	if (!JSVAL_IS_INT(id))
		ret = JS_FALSE;
	else switch (JSVAL_TO_INT(id)) {
	case CACHE_ENTRY_CONTENT: {
		struct fragment *fragment = get_cache_fragment(cached);

		assert(fragment);

		*vp = STRING_TO_JSVAL(JS_NewStringCopyN(smjs_ctx,
	                                                fragment->data,
	                                                fragment->length));

		ret = JS_TRUE;
		break;
	}
	case CACHE_ENTRY_TYPE:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx,
	                                                cached->content_type));

		ret = JS_TRUE;
		break;
	case CACHE_ENTRY_HEAD:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx,
	                                                cached->head));

		ret = JS_TRUE;
		break;
	case CACHE_ENTRY_LENGTH:
		*vp = INT_TO_JSVAL(cached->length);

		ret = JS_TRUE;
		break;
	case CACHE_ENTRY_URI:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx,
		                                        struri(cached->uri)));

		ret = JS_TRUE;
		break;
	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return JS_TRUE in this case
		 * and leave *@vp unchanged.  Do the same here.
		 * (Actually not quite the same, as we already used
		 * @undef_to_jsval.)  */
		ret = JS_TRUE;
		break;
	}

	object_unlock(cached);
	return ret;
}

/* @cache_entry_class.setProperty */
static JSBool
cache_entry_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct cache_entry *cached;
	JSBool ret;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &cache_entry_class, NULL))
		return JS_FALSE;

	cached = JS_GetInstancePrivate(ctx, obj,
				       (JSClass *) &cache_entry_class, NULL);
	if (!cached) return JS_FALSE; /* already detached */

	assert(cache_entry_is_valid(cached));
	if_assert_failed return JS_FALSE;

	/* Get a strong reference to the cache entry to prevent it
	 * from being deleted if some function called below decides to
	 * collect garbage.  After this, all code paths must
	 * eventually unlock the object.  */
	object_lock(cached);

	if (!JSVAL_IS_INT(id))
		ret = JS_FALSE;
	else switch (JSVAL_TO_INT(id)) {
	case CACHE_ENTRY_CONTENT: {
		JSString *jsstr = JS_ValueToString(smjs_ctx, *vp);
		unsigned char *str = JS_GetStringBytes(jsstr);
		size_t len = JS_GetStringLength(jsstr);

		add_fragment(cached, 0, str, len);
		normalize_cache_entry(cached, len);

		ret = JS_TRUE;
		break;
	}
	case CACHE_ENTRY_TYPE: {
		JSString *jsstr = JS_ValueToString(smjs_ctx, *vp);
		unsigned char *str = JS_GetStringBytes(jsstr);

		mem_free_set(&cached->content_type, stracpy(str));

		ret = JS_TRUE;
		break;
	}
	case CACHE_ENTRY_HEAD: {
		JSString *jsstr = JS_ValueToString(smjs_ctx, *vp);
		unsigned char *str = JS_GetStringBytes(jsstr);

		mem_free_set(&cached->head, stracpy(str));

		ret = JS_TRUE;
		break;
	}
	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return JS_TRUE in this case.
		 * Do the same here.  */
		ret = JS_TRUE;
		break;
	}

	object_unlock(cached);
	return ret;
}

/** Pointed to by cache_entry_class.finalize.  SpiderMonkey
 * automatically finalizes all objects before it frees the JSRuntime,
 * so cache_entry.jsobject won't be left dangling.  */
static void
cache_entry_finalize(JSContext *ctx, JSObject *obj)
{
	struct cache_entry *cached;

	assert(JS_InstanceOf(ctx, obj, (JSClass *) &cache_entry_class, NULL));
	if_assert_failed return;

	cached = JS_GetInstancePrivate(ctx, obj,
				       (JSClass *) &cache_entry_class, NULL);

	if (!cached) return;	/* already detached */

	JS_SetPrivate(ctx, obj, NULL); /* perhaps not necessary */
	assert(cached->jsobject == obj);
	if_assert_failed return;
	cached->jsobject = NULL;
}

static const JSClass cache_entry_class = {
	"cache_entry",
	JSCLASS_HAS_PRIVATE,	/* struct cache_entry *; a weak reference */
	JS_PropertyStub, JS_PropertyStub,
	cache_entry_get_property, cache_entry_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, cache_entry_finalize
};

/** Return an SMJS object through which scripts can access @a cached.
 * If there already is such an object, return that; otherwise create a
 * new one.  The SMJS object holds only a weak reference to @a cached;
 * so if the caller wants to guarantee that @a cached exists at least
 * until a script returns, it should use lock_object() and
 * unlock_object().  */
JSObject *
smjs_get_cache_entry_object(struct cache_entry *cached)
{
	JSObject *cache_entry_object;

	if (cached->jsobject) return cached->jsobject;

	assert(smjs_ctx);
	if_assert_failed return NULL;

	cache_entry_object = JS_NewObject(smjs_ctx,
	                                  (JSClass *) &cache_entry_class,
	                                  NULL, NULL);

	if (!cache_entry_object) return NULL;

	if (JS_FALSE == JS_DefineProperties(smjs_ctx, cache_entry_object,
	                               (JSPropertySpec *) cache_entry_props))
		return NULL;

	/* Do this last, so that if any previous step fails, we can
	 * just forget the object and its finalizer won't attempt to
	 * access @cached.  */
	if (JS_FALSE == JS_SetPrivate(smjs_ctx, cache_entry_object, cached)) /* to @cache_entry_class */
		return NULL;

	cached->jsobject = cache_entry_object;
	return cache_entry_object;
}

/** Ensure that no JSObject contains the pointer @a cached.  This is
 * called when the reference count of the cache entry *@a cached is
 * already 0 and it is about to be freed.  If a JSObject was
 * previously attached to the cache entry, the object will remain in
 * memory but it will no longer be able to access the cache entry. */
void
smjs_detach_cache_entry_object(struct cache_entry *cached)
{
	assert(smjs_ctx);
	assert(cached);
	if_assert_failed return;

	if (!cached->jsobject) return;

	assert(JS_GetInstancePrivate(smjs_ctx, cached->jsobject,
				     (JSClass *) &cache_entry_class, NULL)
	       == cached);
	if_assert_failed {}

	JS_SetPrivate(smjs_ctx, cached->jsobject, NULL);
	cached->jsobject = NULL;
}
