/** SpiderMonkey support for both user scripts and web scripts.
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "ecmascript/spidermonkey-shared.h"

/** A shared runtime used for both user scripts (scripting/smjs/) and
 * scripts on web pages (ecmascript/spidermonkey/).
 *
 * SpiderMonkey has bugs that corrupt memory when multiple JSRuntimes
 * are used: https://bugzilla.mozilla.org/show_bug.cgi?id=378918 and
 * perhaps others.  */
JSRuntime *spidermonkey_runtime;

/** A reference count for ::spidermonkey_runtime so that modules using
 * it can be initialized and shut down in arbitrary order.  */
static int spidermonkey_runtime_refcount;

/** Add a reference to ::spidermonkey_runtime, and initialize it if
 * that is the first reference.
 * @return 1 if successful or 0 on error.  If this succeeds, the
 * caller must eventually call spidermonkey_runtime_release().  */
int
spidermonkey_runtime_addref(void)
{
	if (!spidermonkey_runtime) {
		assert(spidermonkey_runtime_refcount == 0);
		if_assert_failed return 0;
		spidermonkey_runtime = JS_NewRuntime(1L * 1024L * 1024L);
		if (!spidermonkey_runtime) return 0;
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
	assert(spidermonkey_runtime);
	if_assert_failed return;

	--spidermonkey_runtime_refcount;
	if (spidermonkey_runtime_refcount == 0) {
		JS_DestroyRuntime(spidermonkey_runtime);
		spidermonkey_runtime = NULL;
	}
}

/** An ELinks-specific replacement for JS_DefineFunctions().
 *
 * @relates spidermonkeyFunctionSpec */
JSBool
spidermonkey_DefineFunctions(JSContext *cx, JSObject *obj,
			     const spidermonkeyFunctionSpec *fs)
{
	for (; fs->name; fs++) {
		if (!JS_DefineFunction(cx, obj, fs->name, fs->call,
				       fs->nargs, 0))
			return JS_FALSE;
	}
	return JS_TRUE;
}

/** An ELinks-specific replacement for JS_InitClass().
 *
 * @relates spidermonkeyFunctionSpec */
JSObject *
spidermonkey_InitClass(JSContext *cx, JSObject *obj,
		       JSObject *parent_proto, JSClass *clasp,
		       JSNative constructor, uintN nargs,
		       JSPropertySpec *ps,
		       const spidermonkeyFunctionSpec *fs,
		       JSPropertySpec *static_ps,
		       const spidermonkeyFunctionSpec *static_fs)
{
	JSObject *proto = JS_InitClass(cx, obj, parent_proto, clasp,
				       constructor, nargs,
				       ps, NULL, static_ps, NULL);

	if (proto == NULL)
		return NULL;

	if (fs) {
		if (!spidermonkey_DefineFunctions(cx, proto, fs))
			return NULL;
	}

	if (static_fs) {
		JSObject *cons_obj = JS_GetConstructor(cx, proto);

		if (cons_obj == NULL)
			return NULL;
		if (!spidermonkey_DefineFunctions(cx, cons_obj, static_fs))
			return NULL;
	}

	return proto;
}
