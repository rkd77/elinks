#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/ProcessingInstruction.h"

static JSBool
ProcessingInstruction_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_PROCESSING_INSTRUCTION_TARGET:
		/* Write me! */
		break;
	case JSP_PROCESSING_INSTRUCTION_DATA:
		/* Write me! */
		break;
	default:
		return Node_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
ProcessingInstruction_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_PROCESSING_INSTRUCTION_DATA:
		/* Write me ! */
		break;
	default:
		return Node_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec ProcessingInstruction_props[] = {
	{ "data",	JSP_PROCESSING_INSTRUCTION_DATA,	JSPROP_ENUMERATE },
	{ "target",	JSP_PROCESSING_INSTRUCTION_TARGET,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec ProcessingInstruction_funcs[] = {
	{ NULL }
};

const JSClass ProcessingInstruction_class = {
	"ProcessingInstruction",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	ProcessingInstruction_getProperty, ProcessingInstruction_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

