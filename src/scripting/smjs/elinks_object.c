/* The "elinks" object */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"
#include "scripting/scripting.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/elinks_object.h"
#include "scripting/smjs/global_object.h"
#include "scripting/smjs/keybinding.h"
#include "session/task.h"


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

static JSBool
elinks_goto_url(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
	unsigned char *url;

	if (argc != 1 || !smjs_ses) {
		*rval = JSVAL_FALSE;

		return JS_TRUE;
	}

	url = jsval_to_string(ctx, &argv[0]);
	if (!*url)
		return JS_FALSE;

	goto_url(smjs_ses, url);

	*rval = JSVAL_TRUE;

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
	{ "goto_url",   elinks_goto_url,        1 },
	{ NULL }
};

static JSObject *
smjs_get_elinks_object(void)
{
	assert(smjs_ctx);
	assert(smjs_global_object);

	return JS_InitClass(smjs_ctx, smjs_global_object, NULL,
	                    (JSClass *) &elinks_class, NULL, 0, NULL,
	                    (JSFunctionSpec *) elinks_funcs, NULL, NULL);
}

void
smjs_init_elinks_object(void)
{
	smjs_elinks_object = smjs_get_elinks_object();

	smjs_init_keybinding_interface();
}

/* If elinks.<method> is defined, call it with the given arguments,
 * store the return value in rval, and return JS_TRUE. Else return JS_FALSE. */
JSBool
smjs_invoke_elinks_object_method(unsigned char *method, jsval argv[], int argc,
                                 jsval *rval)
{
	JSFunction *func;

	assert(smjs_ctx);
	assert(smjs_elinks_object);
	assert(rval);
	assert(argv);

	if (JS_FALSE == JS_GetProperty(smjs_ctx, smjs_elinks_object,
	                               method, rval))
		return JS_FALSE;

	if (JSVAL_VOID == *rval)
		return JS_FALSE;

	func = JS_ValueToFunction(smjs_ctx, *rval);
	assert(func);

	return JS_CallFunction(smjs_ctx, smjs_elinks_object,
		               func, argc, argv, rval);
}
