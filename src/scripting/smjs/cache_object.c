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

static void cache_entry_finalize(JSFreeOp *op, JSObject *obj);

static JSClassOps cache_entry_ops = {
	JS_PropertyStub, nullptr,
	JS_PropertyStub, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr, cache_entry_finalize
};

static const JSClass cache_entry_class = {
	"cache_entry",
	JSCLASS_HAS_PRIVATE,	/* struct cache_entry *; a weak reference */
	&cache_entry_ops
};

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

static bool cache_entry_get_property_content(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool cache_entry_set_property_content(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool cache_entry_get_property_type(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool cache_entry_set_property_type(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool cache_entry_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool cache_entry_get_property_head(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool cache_entry_set_property_head(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool cache_entry_get_property_uri(JSContext *ctx, unsigned int argc, JS::Value *vp);

static const JSPropertySpec cache_entry_props[] = {
	JS_PSGS("content", cache_entry_get_property_content, cache_entry_set_property_content, JSPROP_ENUMERATE),
	JS_PSGS("type", cache_entry_get_property_type, cache_entry_set_property_type, JSPROP_ENUMERATE),
	JS_PSG("length", cache_entry_get_property_length, JSPROP_ENUMERATE),
	JS_PSGS("head", cache_entry_get_property_head, cache_entry_set_property_head, JSPROP_ENUMERATE),
	JS_PSG("uri", cache_entry_get_property_uri, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool
cache_entry_get_property_content(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct cache_entry *cached;
	struct fragment *fragment;
	bool ret;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &cache_entry_class, NULL))
		return false;

	cached = JS_GetInstancePrivate(ctx, hobj,
				       (JSClass *) &cache_entry_class, NULL);
	if (!cached) return false; /* already detached */

	assert(cache_entry_is_valid(cached));
	if_assert_failed return false;

	/* Get a strong reference to the cache entry to prevent it
	 * from being deleted if some function called below decides to
	 * collect garbage.  After this, all code paths must
	 * eventually unlock the object.  */
	object_lock(cached);

	args.rval().setUndefined();

	fragment = get_cache_fragment(cached);

	if (!fragment) {
		ret = false;
	} else {
		args.rval().setString(JS_NewStringCopyN(smjs_ctx, fragment->data, fragment->length));
		ret = true;
	}

	object_unlock(cached);
	return ret;
}

static bool
cache_entry_set_property_content(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct cache_entry *cached;
	JSString *jsstr;
	unsigned char *str;
	size_t len;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &cache_entry_class, NULL))
		return false;

	cached = JS_GetInstancePrivate(ctx, hobj,
				       (JSClass *) &cache_entry_class, NULL);
	if (!cached) return false; /* already detached */

	assert(cache_entry_is_valid(cached));
	if_assert_failed return false;

	/* Get a strong reference to the cache entry to prevent it
	 * from being deleted if some function called below decides to
	 * collect garbage.  After this, all code paths must
	 * eventually unlock the object.  */
	object_lock(cached);

	jsstr = args[0].toString();
	str = JS_EncodeString(smjs_ctx, jsstr);
	len = JS_GetStringLength(jsstr);
	add_fragment(cached, 0, str, len);
	normalize_cache_entry(cached, len);

	object_unlock(cached);
	return true;
}

static bool
cache_entry_get_property_type(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct cache_entry *cached;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &cache_entry_class, NULL))
		return false;

	cached = JS_GetInstancePrivate(ctx, hobj,
				       (JSClass *) &cache_entry_class, NULL);
	if (!cached) return false; /* already detached */

	assert(cache_entry_is_valid(cached));
	if_assert_failed return false;

	/* Get a strong reference to the cache entry to prevent it
	 * from being deleted if some function called below decides to
	 * collect garbage.  After this, all code paths must
	 * eventually unlock the object.  */
	object_lock(cached);
	args.rval().setString(JS_NewStringCopyZ(smjs_ctx, cached->content_type));
	object_unlock(cached);

	return true;
}

static bool
cache_entry_set_property_type(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct cache_entry *cached;
	JSString *jsstr;
	unsigned char *str;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &cache_entry_class, NULL))
		return false;

	cached = JS_GetInstancePrivate(ctx, hobj,
				       (JSClass *) &cache_entry_class, NULL);
	if (!cached) return false; /* already detached */

	assert(cache_entry_is_valid(cached));
	if_assert_failed return false;

	/* Get a strong reference to the cache entry to prevent it
	 * from being deleted if some function called below decides to
	 * collect garbage.  After this, all code paths must
	 * eventually unlock the object.  */
	object_lock(cached);

	jsstr = args[0].toString();
	str = JS_EncodeString(smjs_ctx, jsstr);
	mem_free_set(&cached->content_type, stracpy(str));

	object_unlock(cached);

	return true;
}





/** Pointed to by cache_entry_class.finalize.  SpiderMonkey
 * automatically finalizes all objects before it frees the JSRuntime,
 * so cache_entry.jsobject won't be left dangling.  */
static void
cache_entry_finalize(JSFreeOp *op, JSObject *obj)
{
	struct cache_entry *cached;
#if 0
	assert(JS_InstanceOf(ctx, obj, (JSClass *) &cache_entry_class, NULL));
	if_assert_failed return;
#endif

	cached = JS_GetPrivate(obj);

	if (!cached) return;	/* already detached */

	JS_SetPrivate(obj, NULL); /* perhaps not necessary */
	assert(cached->jsobject == obj);
	if_assert_failed return;
	cached->jsobject = NULL;
}


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
	                                  (JSClass *) &cache_entry_class);

	if (!cache_entry_object) return NULL;

	JS::RootedObject r_cache_entry_object(smjs_ctx, cache_entry_object);

	if (false == JS_DefineProperties(smjs_ctx, r_cache_entry_object,
	                               (JSPropertySpec *) cache_entry_props))
		return NULL;

	/* Do this last, so that if any previous step fails, we can
	 * just forget the object and its finalizer won't attempt to
	 * access @cached.  */
	JS_SetPrivate(cache_entry_object, cached); /* to @cache_entry_class */
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

	JS::RootedObject r_jsobject(smjs_ctx, cached->jsobject);

	assert(JS_GetInstancePrivate(smjs_ctx, r_jsobject,
				     (JSClass *) &cache_entry_class, NULL)
	       == cached);
	if_assert_failed {}

	JS_SetPrivate(cached->jsobject, NULL);
	cached->jsobject = NULL;
}

static bool
cache_entry_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct cache_entry *cached;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &cache_entry_class, NULL))
		return false;

	cached = JS_GetInstancePrivate(ctx, hobj,
				       (JSClass *) &cache_entry_class, NULL);
	if (!cached) return false; /* already detached */

	assert(cache_entry_is_valid(cached));
	if_assert_failed return false;

	/* Get a strong reference to the cache entry to prevent it
	 * from being deleted if some function called below decides to
	 * collect garbage.  After this, all code paths must
	 * eventually unlock the object.  */
	object_lock(cached);
	args.rval().setInt32(cached->length);
	object_unlock(cached);

	return true;
}

static bool
cache_entry_get_property_head(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct cache_entry *cached;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &cache_entry_class, NULL))
		return false;

	cached = JS_GetInstancePrivate(ctx, hobj,
				       (JSClass *) &cache_entry_class, NULL);
	if (!cached) return false; /* already detached */

	assert(cache_entry_is_valid(cached));
	if_assert_failed return false;

	/* Get a strong reference to the cache entry to prevent it
	 * from being deleted if some function called below decides to
	 * collect garbage.  After this, all code paths must
	 * eventually unlock the object.  */
	object_lock(cached);
	args.rval().setString(JS_NewStringCopyZ(smjs_ctx, cached->head));
	object_unlock(cached);

	return true;
}

static bool
cache_entry_set_property_head(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct cache_entry *cached;
	JSString *jsstr;
	unsigned char *str;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &cache_entry_class, NULL))
		return false;

	cached = JS_GetInstancePrivate(ctx, hobj,
				       (JSClass *) &cache_entry_class, NULL);
	if (!cached) return false; /* already detached */

	assert(cache_entry_is_valid(cached));
	if_assert_failed return false;

	/* Get a strong reference to the cache entry to prevent it
	 * from being deleted if some function called below decides to
	 * collect garbage.  After this, all code paths must
	 * eventually unlock the object.  */
	object_lock(cached);

	jsstr = args[0].toString();
	str = JS_EncodeString(smjs_ctx, jsstr);
	mem_free_set(&cached->head, stracpy(str));

	object_unlock(cached);

	return true;
}

static bool
cache_entry_get_property_uri(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct cache_entry *cached;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &cache_entry_class, NULL))
		return false;

	cached = JS_GetInstancePrivate(ctx, hobj,
				       (JSClass *) &cache_entry_class, NULL);
	if (!cached) return false; /* already detached */

	assert(cache_entry_is_valid(cached));
	if_assert_failed return false;

	/* Get a strong reference to the cache entry to prevent it
	 * from being deleted if some function called below decides to
	 * collect garbage.  After this, all code paths must
	 * eventually unlock the object.  */
	object_lock(cached);
	args.rval().setString(JS_NewStringCopyZ(smjs_ctx, struri(cached->uri)));
	object_unlock(cached);

	return true;
}
