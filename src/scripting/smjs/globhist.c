/* "elinks.globhist" */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "globhist/globhist.h"
#include "ecmascript/spidermonkey-shared.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/elinks_object.h"
#include "util/memory.h"

static JSBool smjs_globhist_item_get_property(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSMutableHandleValue hvp);
static JSBool smjs_globhist_get_property(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSMutableHandleValue hvp);
static JSBool smjs_globhist_item_set_property(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSBool strict, JSMutableHandleValue hvp);
static void smjs_globhist_item_finalize(JSFreeOp *op, JSObject *obj);

static const JSClass smjs_globhist_class = {
	"global_history", 0,
	JS_PropertyStub, JS_PropertyStub,
	smjs_globhist_get_property, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL,
};

static const JSClass smjs_globhist_item_class = {
	"global_history_item",
	JSCLASS_HAS_PRIVATE,	/* struct global_history_item * */
	JS_PropertyStub, JS_PropertyStub,
	smjs_globhist_item_get_property, smjs_globhist_item_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	smjs_globhist_item_finalize,
};

/* @smjs_globhist_item_class.finalize */
static void
smjs_globhist_item_finalize(JSFreeOp *op, JSObject *obj)
{
	struct global_history_item *history_item;
#if 0
	assert(JS_InstanceOf(ctx, obj, (JSClass *) &smjs_globhist_item_class, NULL));
	if_assert_failed return;
#endif

	history_item = JS_GetPrivate(obj);

	if (history_item) object_unlock(history_item);
}

/* Tinyids of properties.  Use negative values to distinguish these
 * from array indexes (even though this object has no array elements).
 * ECMAScript code should not use these directly as in global_history_item[-1];
 * future future versions of ELinks may change the numbers.  */
enum smjs_globhist_item_prop {
	GLOBHIST_TITLE      = -1,
	GLOBHIST_URL        = -2,
	GLOBHIST_LAST_VISIT = -3,
};

static JSBool smjs_globhist_item_get_property_title(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSMutableHandleValue hvp);
static JSBool smjs_globhist_item_set_property_title(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSBool strict, JSMutableHandleValue hvp);
static JSBool smjs_globhist_item_get_property_url(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSMutableHandleValue hvp);
static JSBool smjs_globhist_item_set_property_url(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSBool strict, JSMutableHandleValue hvp);
static JSBool smjs_globhist_item_get_property_last_visit(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSMutableHandleValue hvp);
static JSBool smjs_globhist_item_set_property_last_visit(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSBool strict, JSMutableHandleValue hvp);

static const JSPropertySpec smjs_globhist_item_props[] = {
	{ "title",      0, JSPROP_ENUMERATE, JSOP_WRAPPER(smjs_globhist_item_get_property_title), JSOP_WRAPPER(smjs_globhist_item_set_property_title) },
	{ "url",        0, JSPROP_ENUMERATE, JSOP_WRAPPER(smjs_globhist_item_get_property_url), JSOP_WRAPPER(smjs_globhist_item_set_property_url) },
	{ "last_visit", 0, JSPROP_ENUMERATE, JSOP_WRAPPER(smjs_globhist_item_get_property_last_visit), JSOP_WRAPPER(smjs_globhist_item_set_property_last_visit) },
	{ NULL }
};

/* @smjs_globhist_item_class.getProperty */
static JSBool
smjs_globhist_item_get_property(JSContext *ctx, JSHandleObject hobj, JSHandleId hid,
                                JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS
	jsid id = (hid);

	struct global_history_item *history_item;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &smjs_globhist_item_class, NULL))
		return JS_FALSE;

	history_item = JS_GetInstancePrivate(ctx, obj,
					     (JSClass *) &smjs_globhist_item_class,
					     NULL);

	if (!history_item) return JS_FALSE;

	undef_to_jsval(ctx, &vp);

	if (!JSID_IS_INT(id))
		return JS_FALSE;

	switch (JSID_TO_INT(id)) {
	case GLOBHIST_TITLE:
		vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx,
		                                        history_item->title));

		return JS_TRUE;
	case GLOBHIST_URL:
		vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx,
		                                        history_item->url));

		return JS_TRUE;
	case GLOBHIST_LAST_VISIT:
		/* TODO: I'd rather return a date object, but that introduces
		 * synchronisation issues:
		 *
		 *  - How do we cause a change to that date object to affect
		 *    the actual global history item?
		 *  - How do we get a change to that global history item
		 *    to affect all date objects?
		 *
		 * The biggest obstacle is that we have no way to trigger code
		 * when one messes with the date object.
		 *
		 *   -- Miciah */
		/* XXX: Currently, ECMAScript gets seconds since the epoch.
		 * Since the Date object uses milliseconds since the epoch,
		 * I'd rather export that, but SpiderMonkey doesn't provide
		 * a suitable type. -- Miciah */
		vp = JS_NumberValue(history_item->last_visit);

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

/* @smjs_globhist_item_class.setProperty */
static JSBool
smjs_globhist_item_set_property(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSBool strict, JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS
	jsid id = (hid);

	struct global_history_item *history_item;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &smjs_globhist_item_class, NULL))
		return JS_FALSE;

	history_item = JS_GetInstancePrivate(ctx, obj,
					     (JSClass *) &smjs_globhist_item_class,
					     NULL);

	if (!history_item) return JS_FALSE;

	if (!JSID_IS_INT(id))
		return JS_FALSE;

	switch (JSID_TO_INT(id)) {
	case GLOBHIST_TITLE: {
		JSString *jsstr = JS_ValueToString(smjs_ctx, vp);
		unsigned char *str = JS_EncodeString(smjs_ctx, jsstr);

		mem_free_set(&history_item->title, stracpy(str));

		return JS_TRUE;
	}
	case GLOBHIST_URL: {
		JSString *jsstr = JS_ValueToString(smjs_ctx, vp);
		unsigned char *str = JS_EncodeString(smjs_ctx, jsstr);

		mem_free_set(&history_item->url, stracpy(str));

		return JS_TRUE;
	}
	case GLOBHIST_LAST_VISIT: {
		uint32_t seconds;

		/* Bug 923: Assumes time_t values fit in uint32.  */
		JS_ValueToECMAUint32(smjs_ctx, vp, &seconds);
		history_item->last_visit = seconds;

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


static JSObject *
smjs_get_globhist_item_object(struct global_history_item *history_item)
{
	JSObject *jsobj;

	jsobj = JS_NewObject(smjs_ctx, (JSClass *) &smjs_globhist_item_class,
	                     NULL, NULL);
	if (!jsobj
	    || JS_TRUE != JS_DefineProperties(smjs_ctx, jsobj,
	                           (JSPropertySpec *) smjs_globhist_item_props)) {
		return NULL;
	}
	JS_SetPrivate(jsobj, history_item);	/* to @smjs_globhist_item_class */
	object_lock(history_item);

	return jsobj;
}


/* @smjs_globhist_class.getProperty */
static JSBool
smjs_globhist_get_property(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS
	jsid id = (hid);
	(void)obj;

	JSObject *jsobj;
	unsigned char *uri_string;
	struct global_history_item *history_item;
	jsval tmp;

	if (!JS_IdToValue(ctx, id, &tmp))
		goto ret_null;

	uri_string = JS_EncodeString(ctx, JS_ValueToString(ctx, tmp));
	if (!uri_string) goto ret_null;

	history_item = get_global_history_item(uri_string);
	if (!history_item) goto ret_null;

	jsobj = smjs_get_globhist_item_object(history_item);
	if (!jsobj) goto ret_null;

	vp = OBJECT_TO_JSVAL(jsobj);

	return JS_TRUE;

ret_null:
	vp = JSVAL_NULL;

	return JS_TRUE;
}


static JSObject *
smjs_get_globhist_object(void)
{
	JSObject *globhist;

	globhist = JS_NewObject(smjs_ctx, (JSClass *) &smjs_globhist_class,
	                        NULL, NULL);
	if (!globhist) return NULL;

	return globhist;
}

void
smjs_init_globhist_interface(void)
{
	jsval val;
	struct JSObject *globhist;

	if (!smjs_ctx || !smjs_elinks_object)
		return;

	globhist = smjs_get_globhist_object();
	if (!globhist) return;

	val = OBJECT_TO_JSVAL(globhist);

	JS_SetProperty(smjs_ctx, smjs_elinks_object, "globhist", &val);
}

static JSBool
smjs_globhist_item_get_property_title(JSContext *ctx, JSHandleObject hobj, JSHandleId hid,
                                      JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	struct global_history_item *history_item;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &smjs_globhist_item_class, NULL))
		return JS_FALSE;

	history_item = JS_GetInstancePrivate(ctx, obj,
					     (JSClass *) &smjs_globhist_item_class,
					     NULL);

	if (!history_item) return JS_FALSE;

	vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx, history_item->title));

	return JS_TRUE;
}

static JSBool
smjs_globhist_item_set_property_title(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSBool strict, JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	struct global_history_item *history_item;
	JSString *jsstr;
	unsigned char *str;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &smjs_globhist_item_class, NULL))
		return JS_FALSE;

	history_item = JS_GetInstancePrivate(ctx, obj,
					     (JSClass *) &smjs_globhist_item_class,
					     NULL);

	if (!history_item) return JS_FALSE;

	jsstr = JS_ValueToString(smjs_ctx, vp);
	str = JS_EncodeString(smjs_ctx, jsstr);
	mem_free_set(&history_item->title, stracpy(str));

	return JS_TRUE;
}

static JSBool
smjs_globhist_item_get_property_url(JSContext *ctx, JSHandleObject hobj, JSHandleId hid,
                                    JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	struct global_history_item *history_item;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &smjs_globhist_item_class, NULL))
		return JS_FALSE;

	history_item = JS_GetInstancePrivate(ctx, obj,
					     (JSClass *) &smjs_globhist_item_class,
					     NULL);

	if (!history_item) return JS_FALSE;

	vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx, history_item->url));

	return JS_TRUE;
}

static JSBool
smjs_globhist_item_set_property_url(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSBool strict, JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	struct global_history_item *history_item;
	JSString *jsstr;
	unsigned char *str;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &smjs_globhist_item_class, NULL))
		return JS_FALSE;

	history_item = JS_GetInstancePrivate(ctx, obj,
					     (JSClass *) &smjs_globhist_item_class,
					     NULL);

	if (!history_item) return JS_FALSE;

	jsstr = JS_ValueToString(smjs_ctx, vp);
	str = JS_EncodeString(smjs_ctx, jsstr);
	mem_free_set(&history_item->url, stracpy(str));

	return JS_TRUE;
}

static JSBool
smjs_globhist_item_get_property_last_visit(JSContext *ctx, JSHandleObject hobj, JSHandleId hid,
                                           JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	struct global_history_item *history_item;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &smjs_globhist_item_class, NULL))
		return JS_FALSE;

	history_item = JS_GetInstancePrivate(ctx, obj,
					     (JSClass *) &smjs_globhist_item_class,
					     NULL);

	if (!history_item) return JS_FALSE;

	/* TODO: I'd rather return a date object, but that introduces
	 * synchronisation issues:
	 *
	 *  - How do we cause a change to that date object to affect
	 *    the actual global history item?
	 *  - How do we get a change to that global history item
	 *    to affect all date objects?
	 *
	 * The biggest obstacle is that we have no way to trigger code
	 * when one messes with the date object.
	 *
	 *   -- Miciah */
	/* XXX: Currently, ECMAScript gets seconds since the epoch.
	 * Since the Date object uses milliseconds since the epoch,
	 * I'd rather export that, but SpiderMonkey doesn't provide
	 * a suitable type. -- Miciah */
	vp = JS_NumberValue(history_item->last_visit);

	return JS_TRUE;
}

/* @smjs_globhist_item_class.setProperty */
static JSBool
smjs_globhist_item_set_property_last_visit(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSBool strict, JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	struct global_history_item *history_item;
	uint32_t seconds;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &smjs_globhist_item_class, NULL))
		return JS_FALSE;

	history_item = JS_GetInstancePrivate(ctx, obj,
					     (JSClass *) &smjs_globhist_item_class,
					     NULL);

	if (!history_item) return JS_FALSE;

	/* Bug 923: Assumes time_t values fit in uint32.  */
	JS_ValueToECMAUint32(smjs_ctx, vp, &seconds);
	history_item->last_visit = seconds;

	return JS_TRUE;
}
