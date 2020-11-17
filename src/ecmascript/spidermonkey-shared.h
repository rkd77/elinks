#ifndef EL__ECMASCRIPT_SPIDERMONKEY_SHARED_H
#define EL__ECMASCRIPT_SPIDERMONKEY_SHARED_H

/* Tell SpiderMonkey headers which operating system will be used.
 * With standalone SpiderMonkey 1.8.5, "pkg-config --cflags mozjs185"
 * does not define such a macro, so ELinks must do that here.
 * With xulrunner 2.0 however, "pkg-config --cflags mozilla-js"
 * already outputs -DXP_UNIX or similar.  To prevent a warning about
 * macro redefinition, define the macro as 1, like -D does.  */
#ifdef CONFIG_OS_BEOS
#define XP_BEOS 1
#elif CONFIG_OS_OS2
#define XP_OS2 1
#elif CONFIG_OS_RISCOS
#error Out of luck, buddy!
#elif CONFIG_OS_UNIX
#define XP_UNIX 1
#elif CONFIG_OS_WIN32
#define XP_WIN 1
#endif

#include <jsapi.h>

#include "util/string.h"

extern JSRuntime *spidermonkey_runtime;
extern JSContext *main_ctx;
int spidermonkey_runtime_addref(void);
void spidermonkey_runtime_release(void);

/** An ELinks-specific replacement for JSFunctionSpec.
 *
 * Bug 1016: In SpiderMonkey 1.7 bundled with XULRunner 1.8, jsapi.h
 * defines JSFunctionSpec in different ways depending on whether
 * MOZILLA_1_8_BRANCH is defined, and there is no obvious way for
 * ELinks to check whether MOZILLA_1_8_BRANCH was defined when the
 * library was built.  Avoid the unstable JSFunctionSpec definitions
 * and use this ELinks-specific structure instead.  */
typedef struct spidermonkeyFunctionSpec {
	const char *name;
	JSNative call;
	uint8_t nargs;
	/* ELinks does not use "flags" and "extra" so omit them here.  */
} spidermonkeyFunctionSpec;

bool spidermonkey_DefineFunctions(JSContext *cx, JSObject *obj,
				    const spidermonkeyFunctionSpec *fs);
JSObject *spidermonkey_InitClass(JSContext *cx, JSObject *obj,
				 JSObject *parent_proto, JSClass *clasp,
				 JSNative constructor, unsigned int nargs,
				 JSPropertySpec *ps,
				 const spidermonkeyFunctionSpec *fs,
				 JSPropertySpec *static_ps,
				 const spidermonkeyFunctionSpec *static_fs);

static unsigned char *jsval_to_string(JSContext *ctx, JS::HandleValue hvp);
static unsigned char *jsid_to_string(JSContext *ctx, JS::HandleId hid);

/* Inline functions */

static inline unsigned char *
jsval_to_string(JSContext *ctx, JS::HandleValue hvp)
{
//	JS::RootedValue r_vp(ctx, *vp);
	JSString *str = hvp.toString();
//JS::RootedString r_str(ctx, str);

	return empty_string_or_(JS_EncodeString(ctx, str));
}

static inline unsigned char *
jsid_to_string(JSContext *ctx, JS::HandleId hid)
{
	JS::RootedValue v(ctx);

	/* TODO: check returned value */
	JS_IdToValue(ctx, hid, &v);
	return jsval_to_string(ctx, v);
}

#define ELINKS_CAST_PROP_PARAMS	JSObject *obj = (hobj.get()); \
	JS::Value *vp = (hvp.address());

#endif
