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

JS::Heap<JSObject*> *smjs_global_object;

static const JSClassOps global_ops = {
	nullptr, nullptr,
	nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr
};

static const JSClass global_class = {
	"global", JSCLASS_GLOBAL_FLAGS,
	&global_ops
};

static JS::Heap<JSObject*> *
smjs_get_global_object(void)
{
	assert(smjs_ctx);
	JS::RealmOptions opts;

//	if (!JS::InitSelfHostedCode(smjs_ctx)) {
//		return NULL;
//	}

	JS::RootedObject jsobj(smjs_ctx);
	JS::Heap<JSObject*> *global_obj = new JS::Heap<JSObject*>(JS_NewGlobalObject(smjs_ctx, &global_class, NULL, JS::DontFireOnNewGlobalHook, opts));

	jsobj = global_obj->get();
	JSAutoRealm ar(smjs_ctx, jsobj);

	if (!jsobj) return NULL;

	JS::InitRealmStandardClasses(smjs_ctx);

	return global_obj;
}

void
smjs_init_global_object(void)
{
	smjs_global_object = smjs_get_global_object();
}
