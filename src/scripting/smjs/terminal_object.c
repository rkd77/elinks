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
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memory.h"
#include "viewer/text/vs.h"

static bool terminal_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static void terminal_finalize(JSFreeOp *op, JSObject *obj);

static const JSClass terminal_class = {
	"terminal",
	JSCLASS_HAS_PRIVATE, /* struct terminal *; a weak refernce */
	JS_PropertyStub, nullptr,
	terminal_get_property, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr, terminal_finalize
};

enum terminal_prop {
	TERMINAL_TAB,
};

static const JSPropertySpec terminal_props[] = {
	{ "tab", TERMINAL_TAB, JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

/* @terminal_class.getProperty */
static bool
terminal_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	jsid id = hid.get();

	struct terminal *term;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &terminal_class, NULL))
		return false;

	term = JS_GetInstancePrivate(ctx, hobj,
				       (JSClass *) &terminal_class, NULL);
	if (!term) return false; /* already detached */

	hvp.setUndefined();

	if (!JSID_IS_INT(id)) return false;

	switch (JSID_TO_INT(id)) {
	case TERMINAL_TAB: {
		JSObject *obj = smjs_get_session_array_object(term);

		if (obj) {
			hvp.setObject(*obj);
		}

		return true;
	}
	default:
		INTERNAL("Invalid ID %d in terminal_get_property().",
		         JSID_TO_INT(id));
	}

	return false;
}

/** Pointed to by terminal_class.finalize.  SpiderMonkey automatically
 * finalizes all objects before it frees the JSRuntime, so terminal.jsobject
 * won't be left dangling.  */
static void
terminal_finalize(JSFreeOp *op, JSObject *obj)
{
	struct terminal *term;
#if 0
	assert(JS_InstanceOf(ctx, obj, (JSClass *) &terminal_class, NULL));
	if_assert_failed return;
#endif
	term = JS_GetPrivate(obj);

	if (!term) return; /* already detached */

	JS_SetPrivate(obj, NULL); /* perhaps not necessary */
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
	JS_SetPrivate(obj, term); /* to @terminal_class */

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

	assert(JS_GetInstancePrivate(smjs_ctx, r_jsobject,
				     (JSClass *) &terminal_class, NULL)
	       == term);
	if_assert_failed {}

	JS_SetPrivate(term->jsobject, NULL);
	term->jsobject = NULL;
}


/* @terminal_array_class.getProperty */
static bool
terminal_array_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	jsid id = hid.get();

	int index;
	struct terminal *term;

	hvp.setUndefined();

	if (!JSID_IS_INT(id))
		return false;

	index = JSID_TO_INT(id);
	foreach (term, terminals) {
		if (!index) break;
		--index;
	}
	if ((void *) term == (void *) &terminals) return false;

	JSObject *obj = smjs_get_terminal_object(term);
	if (obj) {
		hvp.setObject(*obj);
	}

	return true;
;
}

static const JSClass terminal_array_class = {
	"terminal_array",
	0,
	JS_PropertyStub, nullptr,
	terminal_array_get_property, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr, nullptr
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
	jsval val;
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
