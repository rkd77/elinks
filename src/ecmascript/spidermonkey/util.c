/* Better compatibility across versions of SpiderMonkey. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"

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
