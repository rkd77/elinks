
#ifndef EL__ECMASCRIPT_SPIDERMONKEY_UTIL_H
#define EL__ECMASCRIPT_SPIDERMONKEY_UTIL_H

#include "ecmascript/spidermonkey-shared.h"
#include "util/memory.h"

static void string_to_jsval(JSContext *ctx, JS::Value *vp, unsigned char *string);
static void astring_to_jsval(JSContext *ctx, JS::Value *vp, unsigned char *string);

static int jsval_to_boolean(JSContext *ctx, JS::Value *vp);



/** Inline functions */

static inline void
string_to_jsval(JSContext *ctx, JS::Value *vp, unsigned char *string)
{
	if (!string) {
		*vp = JS::NullValue();
	} else {
		*vp = JS::StringValue(JS_NewStringCopyZ(ctx, string));
	}
}

static inline void
astring_to_jsval(JSContext *ctx, JS::Value *vp, unsigned char *string)
{
	string_to_jsval(ctx, vp, string);
	mem_free_if(string);
}

static inline int
jsval_to_boolean(JSContext *ctx, JS::Value *vp)
{
	JS::RootedValue r_vp(ctx, *vp);
	return (int)r_vp.toBoolean();
}

#endif
