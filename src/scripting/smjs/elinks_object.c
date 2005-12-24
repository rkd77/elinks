/* The "elinks" object */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"
#include "protocol/uri.h"
#include "scripting/scripting.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/elinks_object.h"
#include "scripting/smjs/global_object.h"
#include "scripting/smjs/keybinding.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"


static JSBool
elinks_get_location(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct uri *uri;

	if (!smjs_ses) return JS_FALSE;

	uri = have_location(smjs_ses) ? cur_loc(smjs_ses)->vs.uri
	                              : smjs_ses->loading_uri;
	if (!uri) return JS_FALSE;

	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx, struri(uri)));

	return JS_TRUE;
}

static JSBool
elinks_set_location(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSString *jsstr;
	unsigned char *url;

	if (!smjs_ses) return JS_FALSE;

	jsstr = JS_ValueToString(smjs_ctx, *vp);
	if (!jsstr) return JS_FALSE;

	url = JS_GetStringBytes(jsstr);
	if (!url) return JS_FALSE;

	goto_url(smjs_ses, url);

	return JS_TRUE;
}

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

static JSObject *
smjs_get_elinks_object(void)
{
	JSObject *jsobj;

	assert(smjs_ctx);
	assert(smjs_global_object);

	jsobj = JS_InitClass(smjs_ctx, smjs_global_object, NULL,
	                     (JSClass *) &elinks_class, NULL, 0, NULL,
	                     (JSFunctionSpec *) elinks_funcs, NULL, NULL);

	JS_DefineProperty(smjs_ctx, jsobj, "location", JSVAL_NULL,
	                  elinks_get_location, elinks_set_location,
	                  JSPROP_ENUMERATE | JSPROP_PERMANENT);

	return jsobj;
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
