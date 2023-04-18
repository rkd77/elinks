/* The SpiderMonkey domimplementation object. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/ecmascript.h"
#include "ecmascript/libdom/parse.h"
#include "ecmascript/spidermonkey/document.h"
#include "ecmascript/spidermonkey/implementation.h"
#include "ecmascript/spidermonkey/util.h"
#include "util/conv.h"
#include <jsfriendapi.h>

static JSClassOps implementation_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	nullptr,  // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

static JSClass implementation_class = {
	"implementation",
	JSCLASS_HAS_RESERVED_SLOTS(1),
	&implementation_ops
};

static bool
implementation_createHTMLDocument(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp || argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	if (!JS_InstanceOf(ctx, hobj, &implementation_class, &args)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	char *title = jsval_to_string(ctx, args[0]);

	if (title) {
		struct string str;
		if (!init_string(&str)) {
			mem_free(title);
			args.rval().setNull();
			return true;
		}

		add_to_string(&str, "<!doctype html>\n<html><head><title>");
		add_html_to_string(&str, title, strlen(title));
		add_to_string(&str, "</title></head><body></body></html>");

		void *docu = document_parse_text(str.source, str.length);
		done_string(&str);
		mem_free(title);

		JSObject *obj = getDocument(ctx, docu);

		if (obj) {
			args.rval().setObject(*obj);
			return true;
		}
	}

	args.rval().setNull();
	return true;
}

static const spidermonkeyFunctionSpec implementation_funcs[] = {
	{ "createHTMLDocument",	implementation_createHTMLDocument,	1 },
	{ NULL }
};


JSObject *
getImplementation(JSContext *ctx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSObject *el = JS_NewObject(ctx, &implementation_class);

	if (!el) {
		return NULL;
	}

	JS::RootedObject r_el(ctx, el);
	spidermonkey_DefineFunctions(ctx, el, implementation_funcs);

	return el;
}
