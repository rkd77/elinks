#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/CharacterData.h"
#include "document/dom/ecmascript/spidermonkey/Text.h"

JSBool
Text_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_DOM_IS_ELEMENT_CONTENT_WHITESPACE:
		/* Write me! */
		break;
	case JSP_DOM_WHOLE_TEXT:
		/* Write me! */
		break;
	default:
		return CharacterData_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
Text_splitText(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Text_replaceWholeText(JSContext *ctx, JSObject *obj, uintN argc,
			jsval *argv, jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec Text_props[] = {
	{ "isElementContentWhitespace",	JSP_DOM_IS_ELEMENT_CONTENT_WHITESPACE,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "wholeText",			JSP_DOM_TYPE_NAMESPACE,			JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec Text_funcs[] = {
	{ "splitText",		Text_splitText,		1 },
	{ "replaceWholeText",	Text_replaceWholeText,	1 },
	{ NULL }
};

const JSClass Text_class = {
	"Text",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	Text_getProperty, CharacterData_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

