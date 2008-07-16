/* The global object. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "ecmascript/spidermonkey-shared.h"
#include "scripting/scripting.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/global_object.h"


JSObject *smjs_global_object;


static const JSClass global_class = {
	"global", 0,
	JS_PropertyStub, JS_PropertyStub,
	JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

static JSObject *
smjs_get_global_object(void)
{
	JSObject *jsobj;

	assert(smjs_ctx);

	jsobj  = JS_NewObject(smjs_ctx, (JSClass *) &global_class, NULL, NULL);

	if (!jsobj) return NULL;

	JS_InitStandardClasses(smjs_ctx, jsobj);

	return jsobj;
}

void
smjs_init_global_object(void)
{
	smjs_global_object = smjs_get_global_object();
}
