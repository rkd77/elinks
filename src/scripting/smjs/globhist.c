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

static bool smjs_globhist_item_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static bool smjs_globhist_item_set_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);

static void smjs_globhist_item_finalize(JSFreeOp *op, JSObject *obj);

static const JSClassOps smjs_globhist_item_ops = {
	nullptr, nullptr,
	smjs_globhist_item_get_property, smjs_globhist_item_set_property,
	nullptr, nullptr, nullptr,
	smjs_globhist_item_finalize,
};

static const JSClass smjs_globhist_item_class = {
	"global_history_item",
	JSCLASS_HAS_PRIVATE,	/* struct global_history_item * */
	&smjs_globhist_item_ops
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

static bool smjs_globhist_item_get_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool smjs_globhist_item_set_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool smjs_globhist_item_get_property_url(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool smjs_globhist_item_set_property_url(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool smjs_globhist_item_get_property_last_visit(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool smjs_globhist_item_set_property_last_visit(JSContext *ctx, unsigned int argc, JS::Value *vp);

static const JSPropertySpec smjs_globhist_item_props[] = {
	JS_PSGS("title", smjs_globhist_item_get_property_title, smjs_globhist_item_set_property_title, JSPROP_ENUMERATE),
	JS_PSGS("url", smjs_globhist_item_get_property_url, smjs_globhist_item_set_property_url, JSPROP_ENUMERATE),
	JS_PSGS("last_visit", smjs_globhist_item_get_property_last_visit, smjs_globhist_item_set_property_last_visit, JSPROP_ENUMERATE),
	JS_PS_END
};

/* @smjs_globhist_item_class.getProperty */
static bool
smjs_globhist_item_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid,
                                JS::MutableHandleValue hvp)
{
	jsid id = hid.get();

	struct global_history_item *history_item;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &smjs_globhist_item_class, NULL))
		return false;

	history_item = JS_GetInstancePrivate(ctx, hobj,
					     (JSClass *) &smjs_globhist_item_class,
					     NULL);

	if (!history_item) return false;

	hvp.setUndefined();

	if (!JSID_IS_INT(id))
		return false;

	switch (JSID_TO_INT(id)) {
	case GLOBHIST_TITLE:
		hvp.setString(JS_NewStringCopyZ(smjs_ctx, history_item->title));

		return true;
	case GLOBHIST_URL:
		hvp.setString(JS_NewStringCopyZ(smjs_ctx, history_item->url));

		return true;
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
		hvp.setInt32(history_item->last_visit);

		return true;
	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return true in this case
		 * and leave *@vp unchanged.  Do the same here.
		 * (Actually not quite the same, as we already used
		 * @undef_to_jsval.)  */
		return true;
	}
}

/* @smjs_globhist_item_class.setProperty */
static bool
smjs_globhist_item_set_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	jsid id = hid.get();

	struct global_history_item *history_item;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &smjs_globhist_item_class, NULL))
		return false;

	history_item = JS_GetInstancePrivate(ctx, hobj,
					     (JSClass *) &smjs_globhist_item_class,
					     NULL);

	if (!history_item) return false;

	if (!JSID_IS_INT(id))
		return false;

	switch (JSID_TO_INT(id)) {
	case GLOBHIST_TITLE: {
		JSString *jsstr = hvp.toString();
		char *str = JS_EncodeString(smjs_ctx, jsstr);

		mem_free_set(&history_item->title, stracpy(str));

		return true;
	}
	case GLOBHIST_URL: {
		JSString *jsstr = hvp.toString();
		char *str = JS_EncodeString(smjs_ctx, jsstr);

		mem_free_set(&history_item->url, stracpy(str));

		return true;
	}
	case GLOBHIST_LAST_VISIT: {
		/* Bug 923: Assumes time_t values fit in uint32.  */
		history_item->last_visit = hvp.toInt32();

		return true;
	}
	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return true in this case.
		 * Do the same here.  */
		return true;
	}
}


static JSObject *
smjs_get_globhist_item_object(struct global_history_item *history_item)
{
	JSObject *jsobj;

	jsobj = JS_NewObject(smjs_ctx, (JSClass *) &smjs_globhist_item_class);

	JS::RootedObject r_jsobj(smjs_ctx, jsobj);

	if (!jsobj
	    || true != JS_DefineProperties(smjs_ctx, r_jsobj,
	                           (JSPropertySpec *) smjs_globhist_item_props)) {
		return NULL;
	}
	JS_SetPrivate(jsobj, history_item);	/* to @smjs_globhist_item_class */
	object_lock(history_item);

	return jsobj;
}


/* @smjs_globhist_class.getProperty */
static bool
smjs_globhist_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	jsid id = hid.get();

	JSObject *jsobj;
	char *uri_string;
	struct global_history_item *history_item;
	JS::Value tmp;
	JS::RootedValue r_tmp(ctx, tmp);


	if (!JS_IdToValue(ctx, id, &r_tmp))
		goto ret_null;

	uri_string = JS_EncodeString(ctx, r_tmp.toString());
	if (!uri_string) goto ret_null;

	history_item = get_global_history_item(uri_string);
	if (!history_item) goto ret_null;

	jsobj = smjs_get_globhist_item_object(history_item);
	if (!jsobj) goto ret_null;

	hvp.setObject(*jsobj);

	return true;

ret_null:
	hvp.setNull();

	return true;
}

static const JSClassOps smjs_globhist_ops = {
	nullptr, nullptr,
	smjs_globhist_get_property, nullptr,
	nullptr, nullptr, nullptr, nullptr
};

static const JSClass smjs_globhist_class = {
	"global_history", 0,
	&smjs_globhist_ops
};

static JSObject *
smjs_get_globhist_object(void)
{
	JSObject *globhist;

	globhist = JS_NewObject(smjs_ctx, (JSClass *) &smjs_globhist_class);
	if (!globhist) return NULL;

	return globhist;
}

void
smjs_init_globhist_interface(void)
{
	JS::Value val;
	struct JSObject *globhist;

	if (!smjs_ctx || !smjs_elinks_object)
		return;

	globhist = smjs_get_globhist_object();
	if (!globhist) return;

	JS::RootedObject r_smjs_elinks_object(smjs_ctx, smjs_elinks_object);
	JS::RootedValue r_val(smjs_ctx, val);
	r_val.setObject(*globhist);

	JS_SetProperty(smjs_ctx, r_smjs_elinks_object, "globhist", r_val);
}

static bool
smjs_globhist_item_get_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct global_history_item *history_item;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &smjs_globhist_item_class, NULL))
		return false;

	history_item = JS_GetInstancePrivate(ctx, hobj,
					     (JSClass *) &smjs_globhist_item_class,
					     NULL);

	if (!history_item) return false;

	args.rval().setString(JS_NewStringCopyZ(smjs_ctx, history_item->title));

	return true;
}

static bool
smjs_globhist_item_set_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct global_history_item *history_item;
	JSString *jsstr;
	char *str;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &smjs_globhist_item_class, NULL))
		return false;

	history_item = JS_GetInstancePrivate(ctx, hobj,
					     (JSClass *) &smjs_globhist_item_class,
					     NULL);

	if (!history_item) return false;

	jsstr = args[0].toString();
	str = JS_EncodeString(smjs_ctx, jsstr);
	mem_free_set(&history_item->title, stracpy(str));

	return true;
}

static bool
smjs_globhist_item_get_property_url(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct global_history_item *history_item;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &smjs_globhist_item_class, NULL))
		return false;

	history_item = JS_GetInstancePrivate(ctx, hobj,
					     (JSClass *) &smjs_globhist_item_class,
					     NULL);

	if (!history_item) return false;

	args.rval().setString(JS_NewStringCopyZ(smjs_ctx, history_item->url));

	return true;
}

static bool
smjs_globhist_item_set_property_url(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct global_history_item *history_item;
	JSString *jsstr;
	char *str;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &smjs_globhist_item_class, NULL))
		return false;

	history_item = JS_GetInstancePrivate(ctx, hobj,
					     (JSClass *) &smjs_globhist_item_class,
					     NULL);

	if (!history_item) return false;

	jsstr = args[0].toString();
	str = JS_EncodeString(smjs_ctx, jsstr);
	mem_free_set(&history_item->url, stracpy(str));

	return true;
}

static bool
smjs_globhist_item_get_property_last_visit(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct global_history_item *history_item;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &smjs_globhist_item_class, NULL))
		return false;

	history_item = JS_GetInstancePrivate(ctx, hobj,
					     (JSClass *) &smjs_globhist_item_class,
					     NULL);

	if (!history_item) return false;

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
	args.rval().setInt32(history_item->last_visit);

	return true;
}

/* @smjs_globhist_item_class.setProperty */
static bool
smjs_globhist_item_set_property_last_visit(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct global_history_item *history_item;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &smjs_globhist_item_class, NULL))
		return false;

	history_item = JS_GetInstancePrivate(ctx, hobj,
					     (JSClass *) &smjs_globhist_item_class,
					     NULL);

	if (!history_item) return false;

	/* Bug 923: Assumes time_t values fit in uint32.  */
	history_item->last_visit = args[0].toInt32();

	return true;
}
