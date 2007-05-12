#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/CharacterData.h"
#include "document/dom/ecmascript/spidermonkey/Comment.h"

const JSPropertySpec Comment_props[] = {
	{ NULL }
};

const JSFunctionSpec Comment_funcs[] = {
	{ NULL }
};

const JSClass Comment_class = {
	"Comment",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	CharacterData_getProperty, CharacterData_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

