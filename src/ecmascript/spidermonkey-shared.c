/** SpiderMonkey support for both user scripts and web scripts.
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "ecmascript/spidermonkey-shared.h"
#include <js/Initialization.h>

/** A JSContext that can be used in JS_SetPrivate and JS_GetPrivate
- * when no better one is available.  This context has no global
- * object, so scripts cannot be evaluated in it.
- */

JSContext *main_ctx;

/** A reference count for ::spidermonkey_runtime so that modules using
 * it can be initialized and shut down in arbitrary order.  */
static int spidermonkey_runtime_refcount;

/** Initialize ::spidermonkey_runtime and ::spidermonkey_empty_context.
 * If already initialized, just increment the reference count.
 *
 * @return 1 if successful or 0 on error.  If this succeeds, the
 * caller must eventually call spidermonkey_runtime_release().  */
int
spidermonkey_runtime_addref(void)
{
	if (spidermonkey_runtime_refcount == 0) {

		if (!JS_Init()) {
			return 0;
		}

		main_ctx = JS_NewContext(16 * 1024 * 1024);

		if (!main_ctx) {
			JS_ShutDown();
			return 0;
		}

		if (!JS::InitSelfHostedCode(main_ctx)) {
			JS_DestroyContext(main_ctx);
			JS_ShutDown();
			return 0;
		}
	}

	spidermonkey_runtime_refcount++;
	assert(spidermonkey_runtime_refcount > 0);
	if_assert_failed { spidermonkey_runtime_refcount--; return 0; }
	return 1;
}

/** Release a reference to ::spidermonkey_runtime, and destroy it if
 * that was the last reference.  If spidermonkey_runtime_addref()
 * failed, then this must not be called.  */
void
spidermonkey_runtime_release(void)
{
	assert(spidermonkey_runtime_refcount > 0);

	--spidermonkey_runtime_refcount;
	if (spidermonkey_runtime_refcount == 0) {
		JS_DestroyContext(main_ctx);
		JS_ShutDown();
	}
}

/** An ELinks-specific replacement for JS_DefineFunctions().
 *
 * @relates spidermonkeyFunctionSpec */
bool
spidermonkey_DefineFunctions(JSContext *cx, JSObject *obj,
			     const spidermonkeyFunctionSpec *fs)
{
	JS::RootedObject hobj(cx, obj);
	for (; fs->name; fs++) {
		if (!JS_DefineFunction(cx, hobj, fs->name, fs->call,
				       fs->nargs, 0))
			return false;
	}
	return true;
}

/** An ELinks-specific replacement for JS_InitClass().
 *
 * @relates spidermonkeyFunctionSpec */
JSObject *
spidermonkey_InitClass(JSContext *cx, JSObject *obj,
		       JSObject *parent_proto, JSClass *clasp,
		       JSNative constructor, unsigned int nargs,
		       JSPropertySpec *ps,
		       const spidermonkeyFunctionSpec *fs,
		       JSPropertySpec *static_ps,
		       const spidermonkeyFunctionSpec *static_fs)
{
	JS::RootedObject hobj(cx, obj);
	JS::RootedObject r_parent_proto(cx, parent_proto);
	JSObject *proto = JS_InitClass(cx, hobj, r_parent_proto, clasp,
				       constructor, nargs,
				       ps, NULL, static_ps, NULL);

	if (proto == NULL)
		return NULL;

	if (fs) {
		if (!spidermonkey_DefineFunctions(cx, proto, fs))
			return NULL;
	}

	if (static_fs) {
		JS::RootedObject r_proto(cx, proto);
		JSObject *cons_obj = JS_GetConstructor(cx, r_proto);

		if (cons_obj == NULL)
			return NULL;
		if (!spidermonkey_DefineFunctions(cx, cons_obj, static_fs))
			return NULL;
	}

	return proto;
}
