#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"
#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/CDATASection.h"
#include "document/dom/ecmascript/spidermonkey/CharacterData.h"
#include "document/dom/ecmascript/spidermonkey/Text.h"

const JSPropertySpec CDATASection_props[] = {
	{ NULL }
};

const JSFunctionSpec CDATASection_funcs[] = {
	{ NULL }
};

const JSClass CDATASection_class = {
	"CDATASection",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	Text_getProperty, CharacterData_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

