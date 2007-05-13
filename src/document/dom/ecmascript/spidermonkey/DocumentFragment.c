#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"
#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/DocumentFragment.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"

const JSPropertySpec DocumentFragment_props[] = {
	{ NULL }
};

const JSFunctionSpec DocumentFragment_funcs[] = {
	{ NULL }
};

const JSClass DocumentFragment_class = {
	"DocumentFragment",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	Node_getProperty, Node_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

