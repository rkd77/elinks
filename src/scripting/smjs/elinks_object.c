/* The "elinks" object */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"
#include "scripting/scripting.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/elinks_object.h"


static JSBool
elinks_alert(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	unsigned char *string;

	if (argc != 1)
		return JS_TRUE;

	string = jsval_to_string(ctx, &argv[0]);
	if (!*string)
		return JS_TRUE;

	alert_smjs_error(string);

	undef_to_jsval(ctx, rval);

	return JS_TRUE;
}

static const JSClass elinks_class = {
	"elinks",
	0,
	JS_PropertyStub, JS_PropertyStub,
	JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

static const JSFunctionSpec elinks_funcs[] = {
	{ "alert",	elinks_alert,		1 },
	{ NULL }
};

JSObject *
smjs_get_elinks_object(JSObject *global_object)
{
	assert(smjs_ctx);
	assert(global_object);

	return JS_InitClass(smjs_ctx, global_object, NULL,
	                    (JSClass *) &elinks_class, NULL, 0, NULL,
	                    (JSFunctionSpec *) elinks_funcs, NULL, NULL);
}
