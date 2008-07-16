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


static const JSClass smjs_globhist_item_class; /* defined below */


/* @smjs_globhist_item_class.finalize */
static void
smjs_globhist_item_finalize(JSContext *ctx, JSObject *obj)
{
	struct global_history_item *history_item;

	assert(JS_InstanceOf(ctx, obj, (JSClass *) &smjs_globhist_item_class, NULL));
	if_assert_failed return;

	history_item = JS_GetInstancePrivate(ctx, obj,
					     (JSClass *) &smjs_globhist_item_class,
					     NULL);

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

static const JSPropertySpec smjs_globhist_item_props[] = {
	{ "title",      GLOBHIST_TITLE,      JSPROP_ENUMERATE },
	{ "url",        GLOBHIST_URL,        JSPROP_ENUMERATE },
	{ "last_visit", GLOBHIST_LAST_VISIT, JSPROP_ENUMERATE },
	{ NULL }
};

/* @smjs_globhist_item_class.getProperty */
static JSBool
smjs_globhist_item_get_property(JSContext *ctx, JSObject *obj, jsval id,
                                jsval *vp)
{
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

	undef_to_jsval(ctx, vp);

	if (!JSVAL_IS_INT(id))
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case GLOBHIST_TITLE:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx,
		                                        history_item->title));

		return JS_TRUE;
	case GLOBHIST_URL:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx,
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
		JS_NewNumberValue(smjs_ctx, history_item->last_visit, vp);

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
smjs_globhist_item_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
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

	if (!JSVAL_IS_INT(id))
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case GLOBHIST_TITLE: {
		JSString *jsstr = JS_ValueToString(smjs_ctx, *vp);
		unsigned char *str = JS_GetStringBytes(jsstr);

		mem_free_set(&history_item->title, stracpy(str));

		return JS_TRUE;
	}
	case GLOBHIST_URL: {
		JSString *jsstr = JS_ValueToString(smjs_ctx, *vp);
		unsigned char *str = JS_GetStringBytes(jsstr);

		mem_free_set(&history_item->url, stracpy(str));

		return JS_TRUE;
	}
	case GLOBHIST_LAST_VISIT: {
		uint32 seconds;

		/* Bug 923: Assumes time_t values fit in uint32.  */
		JS_ValueToECMAUint32(smjs_ctx, *vp, &seconds);
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

static const JSClass smjs_globhist_item_class = {
	"global_history_item",
	JSCLASS_HAS_PRIVATE,	/* struct global_history_item * */
	JS_PropertyStub, JS_PropertyStub,
	smjs_globhist_item_get_property, smjs_globhist_item_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	smjs_globhist_item_finalize,
};

static JSObject *
smjs_get_globhist_item_object(struct global_history_item *history_item)
{
	JSObject *jsobj;

	jsobj = JS_NewObject(smjs_ctx, (JSClass *) &smjs_globhist_item_class,
	                     NULL, NULL);
	if (!jsobj
	    || JS_TRUE != JS_DefineProperties(smjs_ctx, jsobj,
	                           (JSPropertySpec *) smjs_globhist_item_props)
	    || JS_TRUE != JS_SetPrivate(smjs_ctx, jsobj, history_item))	/* to @smjs_globhist_item_class */
		return NULL;

	object_lock(history_item);

	return jsobj;
}


/* @smjs_globhist_class.getProperty */
static JSBool
smjs_globhist_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *jsobj;
	unsigned char *uri_string;
	struct global_history_item *history_item;

	uri_string = JS_GetStringBytes(JS_ValueToString(ctx, id));
	if (!uri_string) goto ret_null;

	history_item = get_global_history_item(uri_string);
	if (!history_item) goto ret_null;

	jsobj = smjs_get_globhist_item_object(history_item);
	if (!jsobj) goto ret_null;

	*vp = OBJECT_TO_JSVAL(jsobj);

	return JS_TRUE;

ret_null:
	*vp = JSVAL_NULL;

	return JS_TRUE;
}

static const JSClass smjs_globhist_class = {
	"global_history", 0,
	JS_PropertyStub, JS_PropertyStub,
	smjs_globhist_get_property, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

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
