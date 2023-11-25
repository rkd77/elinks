/* Exports struct terminal to the world of ECMAScript */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/elinks_object.h"
#include "scripting/smjs/session_object.h"
#include "scripting/smjs/smjs.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memory.h"
#include "viewer/text/vs.h"

static void terminal_finalize(JS::GCContext *op, JSObject *obj);

static const JSClassOps terminal_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	terminal_finalize,  // finalize
	nullptr,  // call
	nullptr,  // construct
	nullptr // trace JS_GlobalObjectTraceHook
};

static const JSClass terminal_class = {
	"terminal",
	JSCLASS_HAS_RESERVED_SLOTS(1), /* struct terminal *; a weak refernce */
	&terminal_ops
};

enum terminal_prop {
	TERMINAL_TAB,
};


static bool
terminal_get_property_tab(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct terminal *term;

	term = JS::GetMaybePtrFromReservedSlot<struct terminal>(hobj, 0);
	if (!term) return false; /* already detached */

	JSObject *obj = smjs_get_session_array_object(term);

	if (obj) {
		args.rval().setObject(*obj);
	} else {
		args.rval().setUndefined();
	}

	return true;
}

static const JSPropertySpec terminal_props[] = {
	JS_PSG("tab",	terminal_get_property_tab, JSPROP_ENUMERATE),
	JS_PS_END
};

/** Pointed to by terminal_class.finalize.  SpiderMonkey automatically
 * finalizes all objects before it frees the JSRuntime, so terminal.jsobject
 * won't be left dangling.  */
static void
terminal_finalize(JS::GCContext *op, JSObject *obj)
{
	struct terminal *term;
	term = JS::GetMaybePtrFromReservedSlot<struct terminal>(obj, 0);

	if (!term) return; /* already detached */

	JS::SetReservedSlot(obj, 0, JS::UndefinedValue()); /* perhaps not necessary */
	assert(term->jsobject == obj);
	if_assert_failed return;
	term->jsobject = NULL;
}


/** Return an SMJS object through which scripts can access @a term.
 * If there already is such an object, return that; otherwise create a
 * new one.  The SMJS object holds only a weak reference to @a term.  */
JSObject *
smjs_get_terminal_object(struct terminal *term)
{
	JSObject *obj;

	if (term->jsobject) return term->jsobject;

	assert(smjs_ctx);
	if_assert_failed return NULL;

	obj = JS_NewObject(smjs_ctx, (JSClass *) &terminal_class);

	if (!obj) return NULL;

	JS::RootedObject robj(smjs_ctx, obj);
	if (false == JS_DefineProperties(smjs_ctx, robj,
	                               (JSPropertySpec *) terminal_props))
		return NULL;

	/* Do this last, so that if any previous step fails, we can
	 * just forget the object and its finalizer won't attempt to
	 * access @cached.  */
	JS::SetReservedSlot(obj, 0, JS::PrivateValue(term)); /* to @terminal_class */

	term->jsobject = obj;
	return obj;
}

/** Ensure that no JSObject contains the pointer @a term.  This is called from
 * destroy_terminal before @a term is freed.  If a JSObject was previously
 * attached to the terminal object, the object will remain in memory but it
 * will no longer be able to access the terminal object. */
void
smjs_detach_terminal_object(struct terminal *term)
{
	assert(smjs_ctx);
	assert(term);
	if_assert_failed return;

	smjs_detach_session_array_object(term);

	if (!term->jsobject) return;

	JS::RootedObject r_jsobject(smjs_ctx, term->jsobject);
	JS::SetReservedSlot(term->jsobject, 0, JS::UndefinedValue());
	term->jsobject = NULL;
}

static const JSClassOps terminal_array_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	nullptr,  // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

static const JSClass terminal_array_class = {
	"terminal_array",
	0,
	&terminal_array_ops
};

/** Return an SMJS object that scripts can use an array to get terminal
 * objects. */
static JSObject *
smjs_get_terminal_array_object(void)
{
	assert(smjs_ctx);
	if_assert_failed return NULL;

	return JS_NewObject(smjs_ctx, (JSClass *) &terminal_array_class);
}

void
smjs_init_terminal_interface(void)
{
	JS::Value val;
	struct JSObject *obj;

	if (!smjs_ctx || !smjs_elinks_object)
		return;

	obj = smjs_get_terminal_array_object();
	if (!obj) return;

	JS::RootedValue rval(smjs_ctx, val);
	rval.setObject(*obj);
	JS::RootedObject r_smjs_elinks_object(smjs_ctx, smjs_elinks_object);

	JS_SetProperty(smjs_ctx, r_smjs_elinks_object, "terminal", rval);
}
