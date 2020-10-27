/* The global object. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "ecmascript/spidermonkey-shared.h"
#include "scripting/scripting.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/global_object.h"

using namespace JS;

JSObject *smjs_global_object;

static const JSClassOps global_ops = {
	JS_PropertyStub, nullptr,
	JS_PropertyStub, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr, nullptr
};

static const JSClass global_class = {
	"global", JSCLASS_GLOBAL_FLAGS,
	&global_ops
};

static JSObject *
smjs_get_global_object(void)
{
	assert(smjs_ctx);
	JSAutoCompartment *acc = NULL;

	JSAutoRequest ar(smjs_ctx);
	JS::CompartmentOptions opts;

	if (!JS::InitSelfHostedCode(smjs_ctx)) {
		return NULL;
	}

	JS::RootedObject jsobj(smjs_ctx, JS_NewGlobalObject(smjs_ctx, (JSClass *) &global_class, NULL, JS::DontFireOnNewGlobalHook, opts));

	if (!jsobj) return NULL;

	acc = new JSAutoCompartment(smjs_ctx, jsobj);

	JS_InitStandardClasses(smjs_ctx, jsobj);

	return jsobj;
}

void
smjs_init_global_object(void)
{
	smjs_global_object = smjs_get_global_object();
}
