/* The "elinks" object */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"
#include "scripting/scripting.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/elinks_object.h"


static const JSClass elinks_class = {
	"elinks",
	0,
	JS_PropertyStub, JS_PropertyStub,
	JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

JSObject *
smjs_get_elinks_object(JSObject *global_object)
{
	return JS_InitClass(smjs_ctx, global_object, NULL,
	                    (JSClass *) &elinks_class, NULL, 0, NULL,
	                    NULL, NULL, NULL);
}
