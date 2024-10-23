#ifndef EL__JS_SPIDERMONKEY_SHARED_H
#define EL__JS_SPIDERMONKEY_SHARED_H

#include <jsapi.h>
#include <jsfriendapi.h>
#include <js/Conversions.h>

#include "util/string.h"

extern JSRuntime *spidermonkey_runtime;
extern JSContext *main_ctx;
extern long spidermonkey_memory_limit;
int spidermonkey_runtime_addref(void);
void spidermonkey_runtime_release(void);
void spidermonkey_release_all_runtimes(void);

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
				 JSObject *parent_proto, const JSClass *clasp,
				 JSNative constructor, unsigned int nargs,
				 JSPropertySpec *ps,
				 const spidermonkeyFunctionSpec *fs,
				 JSPropertySpec *static_ps,
				 const spidermonkeyFunctionSpec *static_fs,
				 const char *name);

bool spidermonkey_check_if_function_name(const spidermonkeyFunctionSpec funcs[], const char *string);

static char *jsval_to_string(JSContext *ctx, JS::HandleValue hvp);
static char *jsid_to_string(JSContext *ctx, JS::HandleId hid);

/* Inline functions */

static inline char *
jsval_to_string(JSContext *ctx, JS::HandleValue hvp)
{
/* Memory must be freed in caller */
	JSString *st = JS::ToString(ctx, hvp);
	JS::RootedString rst(ctx, st);
	JS::UniqueChars utf8chars = JS_EncodeStringToUTF8(ctx, rst);

	return null_or_stracpy(utf8chars.get());
}

static inline char *
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
