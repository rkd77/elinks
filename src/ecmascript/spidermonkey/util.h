
#ifndef EL__ECMASCRIPT_SPIDERMONKEY_UTIL_H
#define EL__ECMASCRIPT_SPIDERMONKEY_UTIL_H

#include "ecmascript/spidermonkey-shared.h"
#include "util/memory.h"

static void string_to_jsval(JSContext *ctx, JS::Value *vp, char *string);
static void astring_to_jsval(JSContext *ctx, JS::Value *vp, char *string);

static int jsval_to_boolean(JSContext *ctx, JS::Value *vp);
static unsigned char * jshandle_value_to_char_string(JSContext *ctx, JS::MutableHandleValue *obj);



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
unsigned char *
jshandle_value_to_char_string(JSContext *ctx, JS::MutableHandleValue *obj)
{
	unsigned char *ret;
	ret = stracpy("");
	if (obj->isString())
	{
		ret = JS_EncodeString(ctx, obj->toString());
	} else if (obj->isNumber())
	{
		int tmpinta = obj->toNumber();
		char tmpints[256]="";
		sprintf(tmpints,"%d",tmpinta);
		add_to_strn(&ret,tmpints);
	} else if (obj->isBoolean())
	{
		int tmpinta = obj->toBoolean();
		char tmpints[16]="";
		sprintf(tmpints,"%d",tmpinta);
		add_to_strn(&ret,tmpints);
	}
	return(ret);
}

#endif
