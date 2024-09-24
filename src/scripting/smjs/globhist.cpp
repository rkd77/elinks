/* "elinks.globhist" */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "globhist/globhist.h"
#include "js/spidermonkey-shared.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/elinks_object.h"
#include "util/memory.h"

//static bool smjs_globhist_item_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
//static bool smjs_globhist_item_set_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);

static void smjs_globhist_item_finalize(JS::GCContext *op, JSObject *obj);

static const JSClassOps smjs_globhist_item_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	smjs_globhist_item_finalize,  // finalize
	nullptr,  // call
	nullptr,  // construct
	nullptr // trace JS_GlobalObjectTraceHook
};

static const JSClass smjs_globhist_item_class = {
	"global_history_item",
	JSCLASS_HAS_RESERVED_SLOTS(1),	/* struct global_history_item * */
	&smjs_globhist_item_ops
};

/* @smjs_globhist_item_class.finalize */
static void
smjs_globhist_item_finalize(JS::GCContext *op, JSObject *obj)
{
	struct global_history_item *history_item;

	history_item = JS::GetMaybePtrFromReservedSlot<struct global_history_item>(obj, 0);

	if (history_item) object_unlock(history_item);
}

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

#if 0
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
	JS::SetReservedSlot(jsobj, 0, JS::PrivateValue(history_item));	/* to @smjs_globhist_item_class */
	object_lock(history_item);

	return jsobj;
}
#endif

#if 0
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

	uri_string = jsval_to_string(ctx, r_tmp);
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
#endif

static const JSClassOps smjs_globhist_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	nullptr,  // finalize
	nullptr,  // call
	nullptr,  // construct
	nullptr // trace JS_GlobalObjectTraceHook
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

	history_item = JS::GetMaybePtrFromReservedSlot<struct global_history_item>(hobj, 0);

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
	char *str;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &smjs_globhist_item_class, NULL))
		return false;

	history_item = JS::GetMaybePtrFromReservedSlot<struct global_history_item>(hobj, 0);

	if (!history_item) return false;

	str = jsval_to_string(smjs_ctx, args[0]);
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

	history_item = JS::GetMaybePtrFromReservedSlot<struct global_history_item>(hobj, 0);

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
	char *str;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &smjs_globhist_item_class, NULL))
		return false;

	history_item = JS::GetMaybePtrFromReservedSlot<struct global_history_item>(hobj, 0);

	if (!history_item) return false;

	str = jsval_to_string(smjs_ctx, args[0]);
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

	history_item = JS::GetMaybePtrFromReservedSlot<struct global_history_item>(hobj, 0);

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

	history_item = JS::GetMaybePtrFromReservedSlot<struct global_history_item>(hobj, 0);

	if (!history_item) return false;

	/* Bug 923: Assumes time_t values fit in uint32.  */
	history_item->last_visit = args[0].toInt32();

	return true;
}
