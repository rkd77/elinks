#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"
#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/CharacterData.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"

JSBool
CharacterData_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_CHARACTER_DATA_DATA:
		/* Write me! */
		break;
	case JSP_CHARACTER_DATA_LENGTH:
		/* Write me! */
		break;
	default:
		return Node_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

JSBool
CharacterData_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_CHARACTER_DATA_DATA:
		/* Write me! */
		break;
	default:
		return Node_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
CharacterData_substringData(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
CharacterData_appendData(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
CharacterData_insertData(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
CharacterData_deleteData(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
CharacterData_replaceData(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec CharacterData_props[] = {
	{ "data",	JSP_CHARACTER_DATA_DATA,	JSPROP_ENUMERATE },
	{ "length",	JSP_CHARACTER_DATA_LENGTH,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec CharacterData_funcs[] = {
	{ "substringData",	CharacterData_substringData,	2 },
	{ "appendData",		CharacterData_appendData,	1 },
	{ "insertData",		CharacterData_insertData,	2 },
	{ "deleteData",		CharacterData_deleteData,	2 },
	{ "replaceData",	CharacterData_replaceData,	3 },
	{ NULL }
};

const JSClass CharacterData_class = {
	"CharacterData",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	CharacterData_getProperty, CharacterData_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

