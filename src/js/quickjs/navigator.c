/* The Quickjs navigator object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/ecmascript.h"
#include "js/quickjs.h"
#include "js/quickjs/navigator.h"
#include "intl/libintl.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "util/conv.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_navigator_class_id;

/* @navigator_class.getProperty */

static JSValue
js_navigator_get_property_appCodeName(JSContext *ctx, JSValueConst this_val)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	JSValue r = JS_NewString(ctx, "Mozilla"); /* More like a constant nowadays. */
	RETURN_JS(r);
}

static JSValue
js_navigator_get_property_appName(JSContext *ctx, JSValueConst this_val)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	JSValue r = JS_NewString(ctx, "ELinks (roughly compatible with Netscape Navigator, Mozilla and Microsoft Internet Explorer)");
	RETURN_JS(r);
}

static JSValue
js_navigator_get_property_appVersion(JSContext *ctx, JSValueConst this_val)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	JSValue r = JS_NewString(ctx, VERSION);
	RETURN_JS(r);
}

static JSValue
js_navigator_get_property_language(JSContext *ctx, JSValueConst this_val)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

#ifdef CONFIG_NLS
	if (get_opt_bool("protocol.http.accept_ui_language", NULL)) {
		JSValue r = JS_NewString(ctx, language_to_iso639(current_language));
		RETURN_JS(r);
	}
#endif
	return JS_UNDEFINED;
}

static JSValue
js_navigator_get_property_platform(JSContext *ctx, JSValueConst this_val)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	JSValue r = JS_NewString(ctx, system_name);
	RETURN_JS(r);
}

static JSValue
js_navigator_get_property_userAgent(JSContext *ctx, JSValueConst this_val)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	char *optstr = get_opt_str("protocol.http.user_agent", NULL);

	if (*optstr && strcmp(optstr, " ")) {
		char *ustr, ts[64] = "";
		static char custr[256];
		/* TODO: Somehow get the terminal in which the
		 * document is actually being displayed.  */
		struct terminal *term = get_default_terminal();

		if (term) {
			unsigned int tslen = 0;

			ulongcat(ts, &tslen, term->width, 3, 0);
			ts[tslen++] = 'x';
			ulongcat(ts, &tslen, term->height, 3, 0);
		}
		ustr = subst_user_agent(optstr, VERSION_STRING, system_name, ts);

		if (ustr) {
			safe_strncpy(custr, ustr, 256);
			mem_free(ustr);

			JSValue r = JS_NewString(ctx, custr);
			RETURN_JS(r);
		}
	}
	JSValue rr = JS_NewString(ctx, system_name);
	RETURN_JS(rr);
}

static JSValue
js_navigator_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[navigator object]");
}

static const JSCFunctionListEntry js_navigator_proto_funcs[] = {
	JS_CGETSET_DEF("appCodeName", js_navigator_get_property_appCodeName, NULL),
	JS_CGETSET_DEF("appName", js_navigator_get_property_appName, NULL),
	JS_CGETSET_DEF("appVersion", js_navigator_get_property_appVersion, NULL),
	JS_CGETSET_DEF("language", js_navigator_get_property_language, NULL),
	JS_CGETSET_DEF("platform", js_navigator_get_property_platform, NULL),
	JS_CGETSET_DEF("userAgent", js_navigator_get_property_userAgent, NULL),
	JS_CFUNC_DEF("toString", 0, js_navigator_toString)
};

static JSClassDef js_navigator_class = {
    "navigator",
};

int
js_navigator_init(JSContext *ctx)
{
	ELOG
	JSValue navigator_proto;

	/* create the navigator class */
	JS_NewClassID(JS_GetRuntime(ctx), &js_navigator_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_navigator_class_id, &js_navigator_class);

	JSValue global_obj = JS_GetGlobalObject(ctx);
	REF_JS(global_obj);

	navigator_proto = JS_NewObject(ctx);
	REF_JS(navigator_proto);

	JS_SetPropertyFunctionList(ctx, navigator_proto, js_navigator_proto_funcs, countof(js_navigator_proto_funcs));
	JS_SetClassProto(ctx, js_navigator_class_id, navigator_proto);
	JS_SetPropertyStr(ctx, global_obj, "navigator", JS_DupValue(ctx, navigator_proto));

	JS_FreeValue(ctx, global_obj);

	return 0;
}
