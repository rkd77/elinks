/* The SiderMonkey DOMRect implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"
#include <js/BigInt.h>
#include <js/Conversions.h>

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/libdom/dom.h"
#include "ecmascript/spidermonkey.h"
#include "ecmascript/spidermonkey/domrect.h"
#include "ecmascript/spidermonkey/heartbeat.h"

#include <iostream>
#include <list>
#include <map>
#include <utility>
#include <sstream>
#include <vector>

static bool domRect_get_property_x(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool domRect_get_property_y(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool domRect_get_property_width(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool domRect_get_property_height(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool domRect_get_property_top(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool domRect_get_property_right(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool domRect_get_property_bottom(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool domRect_get_property_left(JSContext *ctx, unsigned int argc, JS::Value *vp);

static bool domRect_set_property_x(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool domRect_set_property_y(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool domRect_set_property_width(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool domRect_set_property_height(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool domRect_set_property_top(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool domRect_set_property_right(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool domRect_set_property_bottom(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool domRect_set_property_left(JSContext *ctx, unsigned int argc, JS::Value *vp);

struct eljs_domrect {
	float x;
	float y;
	float width;
	float height;
	float top;
	float right;
	float bottom;
	float left;
};

static void
domRect_finalize(JS::GCContext *op, JSObject *domRect_obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	eljs_domrect *d = JS::GetMaybePtrFromReservedSlot<eljs_domrect>(domRect_obj, 0);

	if (d) {
		mem_free(d);
	}
}

JSClassOps domRect_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	domRect_finalize, // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook // trace
};

JSClass domRect_class = {
	"DOMRect",
	JSCLASS_HAS_RESERVED_SLOTS(1),
	&domRect_ops
};

JSPropertySpec domRect_props[] = {
	JS_PSGS("bottom",	domRect_get_property_bottom, domRect_set_property_bottom, JSPROP_ENUMERATE),
	JS_PSGS("height",	domRect_get_property_height, domRect_set_property_height, JSPROP_ENUMERATE),
	JS_PSGS("left",	domRect_get_property_left, domRect_set_property_left, JSPROP_ENUMERATE),
	JS_PSGS("right",	domRect_get_property_right, domRect_set_property_right, JSPROP_ENUMERATE),
	JS_PSGS("top",	domRect_get_property_top, domRect_set_property_top, JSPROP_ENUMERATE),
	JS_PSGS("width",	domRect_get_property_width, domRect_set_property_width, JSPROP_ENUMERATE),
	JS_PSGS("x",	domRect_get_property_x, domRect_set_property_x, JSPROP_ENUMERATE),
	JS_PSGS("y",	domRect_get_property_y, domRect_set_property_y, JSPROP_ENUMERATE),
	JS_PS_END
};

const spidermonkeyFunctionSpec domRect_funcs[] = {
	{ NULL }
};

static bool
domRect_get_property_bottom(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_domrect *d = JS::GetMaybePtrFromReservedSlot<eljs_domrect>(hobj, 0);

	if (!d) {
		return false;
	}
	args.rval().setNumber(d->bottom);

	return true;
}

static bool
domRect_get_property_height(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_domrect *d = JS::GetMaybePtrFromReservedSlot<eljs_domrect>(hobj, 0);

	if (!d) {
		return false;
	}
	args.rval().setNumber(d->height);

	return true;
}

static bool
domRect_get_property_left(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_domrect *d = JS::GetMaybePtrFromReservedSlot<eljs_domrect>(hobj, 0);

	if (!d) {
		return false;
	}
	args.rval().setNumber(d->left);

	return true;
}

static bool
domRect_get_property_right(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_domrect *d = JS::GetMaybePtrFromReservedSlot<eljs_domrect>(hobj, 0);

	if (!d) {
		return false;
	}
	args.rval().setNumber(d->right);

	return true;
}

static bool
domRect_get_property_top(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_domrect *d = JS::GetMaybePtrFromReservedSlot<eljs_domrect>(hobj, 0);

	if (!d) {
		return false;
	}
	args.rval().setNumber(d->top);

	return true;
}

static bool
domRect_get_property_width(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_domrect *d = JS::GetMaybePtrFromReservedSlot<eljs_domrect>(hobj, 0);

	if (!d) {
		return false;
	}
	args.rval().setNumber(d->width);

	return true;
}

static bool
domRect_get_property_x(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_domrect *d = JS::GetMaybePtrFromReservedSlot<eljs_domrect>(hobj, 0);

	if (!d) {
		return false;
	}
	args.rval().setNumber(d->x);

	return true;
}

static bool
domRect_get_property_y(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_domrect *d = JS::GetMaybePtrFromReservedSlot<eljs_domrect>(hobj, 0);

	if (!d) {
		return false;
	}
	args.rval().setNumber(d->y);

	return true;
}

static bool
domRect_set_property_bottom(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_domrect *d = JS::GetMaybePtrFromReservedSlot<eljs_domrect>(hobj, 0);

	if (!d) {
		return false;
	}
	d->bottom = args[0].toNumber();
	args.rval().setUndefined();

	return true;
}

static bool
domRect_set_property_height(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_domrect *d = JS::GetMaybePtrFromReservedSlot<eljs_domrect>(hobj, 0);

	if (!d) {
		return false;
	}
	d->height = args[0].toNumber();
	args.rval().setUndefined();

	return true;
}

static bool
domRect_set_property_left(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_domrect *d = JS::GetMaybePtrFromReservedSlot<eljs_domrect>(hobj, 0);

	if (!d) {
		return false;
	}
	d->left = args[0].toNumber();
	args.rval().setUndefined();

	return true;
}

static bool
domRect_set_property_right(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_domrect *d = JS::GetMaybePtrFromReservedSlot<eljs_domrect>(hobj, 0);

	if (!d) {
		return false;
	}
	d->right = args[0].toNumber();
	args.rval().setUndefined();

	return true;
}

static bool
domRect_set_property_top(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_domrect *d = JS::GetMaybePtrFromReservedSlot<eljs_domrect>(hobj, 0);

	if (!d) {
		return false;
	}
	d->top = args[0].toNumber();
	args.rval().setUndefined();

	return true;
}

static bool
domRect_set_property_width(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_domrect *d = JS::GetMaybePtrFromReservedSlot<eljs_domrect>(hobj, 0);

	if (!d) {
		return false;
	}
	d->width = args[0].toNumber();
	args.rval().setUndefined();

	return true;
}

static bool
domRect_set_property_x(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_domrect *d = JS::GetMaybePtrFromReservedSlot<eljs_domrect>(hobj, 0);

	if (!d) {
		return false;
	}
	d->x = args[0].toNumber();
	args.rval().setUndefined();

	return true;
}

static bool
domRect_set_property_y(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_domrect *d = JS::GetMaybePtrFromReservedSlot<eljs_domrect>(hobj, 0);

	if (!d) {
		return false;
	}
	d->y = args[0].toNumber();
	args.rval().setUndefined();

	return true;
}

JSObject *
getDomRect(JSContext *ctx)
{
	struct eljs_domrect *d = mem_calloc(1, sizeof(*d));

	if (!d) {
		return NULL;
	}
	JSObject *dr = JS_NewObject(ctx, &domRect_class);

	if (!dr) {
		return NULL;
	}
	JS::RootedObject r_domrect(ctx, dr);
	JS_DefineProperties(ctx, r_domrect, (JSPropertySpec *)domRect_props);
	JS::SetReservedSlot(dr, 0, JS::PrivateValue(d));

	return dr;
}
