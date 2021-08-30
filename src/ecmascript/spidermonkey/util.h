#ifndef EL__ECMASCRIPT_SPIDERMONKEY_UTIL_H
#define EL__ECMASCRIPT_SPIDERMONKEY_UTIL_H

#include "ecmascript/spidermonkey-shared.h"
#include "util/memory.h"

static void string_to_jsval(JSContext *ctx, JS::Value *vp, char *string);
static void astring_to_jsval(JSContext *ctx, JS::Value *vp, char *string);

static int jsval_to_boolean(JSContext *ctx, JS::Value *vp);
static void jshandle_value_to_char_string(struct string *string, JSContext *ctx, JS::HandleValue obj);

/** Inline functions */

static inline void
string_to_jsval(JSContext *ctx, JS::Value *vp, char *string)
{
	if (!string) {
		*vp = JS::NullValue();
	} else {
		*vp = JS::StringValue(JS_NewStringCopyZ(ctx, string));
	}
}

static inline void
astring_to_jsval(JSContext *ctx, JS::Value *vp, char *string)
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


/* Since SpiderMonkey 52 the Mutable Handle Object
 * is different for String and Number and must be
 * handled accordingly */
void
jshandle_value_to_char_string(struct string *string, JSContext *ctx, JS::HandleValue obj)
{
	init_string(string);

	if (obj.isString())
	{
		add_to_string(string, jsval_to_string(ctx, obj));
	} else if (obj.isNumber())
	{
		int tmpinta = obj.toNumber();
		add_format_to_string(string, "%d", tmpinta);
	} else if (obj.isBoolean())
	{
		int tmpinta = obj.toNumber();
		add_format_to_string(string, "%d", tmpinta);
	}
}

#endif
