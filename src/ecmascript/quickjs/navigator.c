/* The Quickjs navigator object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/quickjs/navigator.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_navigator_class_id;

/* @navigator_class.getProperty */

static JSValue
js_navigator_get_property_appCodeName(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return JS_NewString(ctx, "Mozilla"); /* More like a constant nowadays. */
}

static JSValue
js_navigator_get_property_appName(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return JS_NewString(ctx, "ELinks (roughly compatible with Netscape Navigator, Mozilla and Microsoft Internet Explorer)");
}

static JSValue
js_navigator_get_property_appVersion(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return JS_NewString(ctx, VERSION);
}

static JSValue
js_navigator_get_property_language(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
#ifdef CONFIG_NLS
	if (get_opt_bool("protocol.http.accept_ui_language", NULL)) {
		return JS_NewString(ctx, language_to_iso639(current_language));
	}
#endif
	return JS_UNDEFINED;
}

static JSValue
js_navigator_get_property_platform(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return JS_NewString(ctx, system_name);
}

static JSValue
js_navigator_get_property_userAgent(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	char *optstr;

	optstr = get_opt_str("protocol.http.user_agent", NULL);

	if (*optstr && strcmp(optstr, " ")) {
		char *ustr, ts[64] = "";
		static unsigned char custr[256];
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

			return JS_NewString(ctx, custr);
		}
	}
	return JS_NewString(ctx, system_name);
}

static const JSCFunctionListEntry js_navigator_proto_funcs[] = {
	JS_CGETSET_DEF("appCodeName", js_navigator_get_property_appCodeName, nullptr),
	JS_CGETSET_DEF("appName", js_navigator_get_property_appName, nullptr),
	JS_CGETSET_DEF("appVersion", js_navigator_get_property_appVersion, nullptr),
	JS_CGETSET_DEF("language", js_navigator_get_property_language, nullptr),
	JS_CGETSET_DEF("platform", js_navigator_get_property_platform, nullptr),
	JS_CGETSET_DEF("userAgent", js_navigator_get_property_userAgent, nullptr),
};

static JSClassDef js_navigator_class = {
    "navigator",
};

static JSValue
js_navigator_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_navigator_class_id);
	JS_FreeValue(ctx, proto);

	if (JS_IsException(obj)) {
		goto fail;
	}
	return obj;

fail:
	JS_FreeValue(ctx, obj);
	return JS_EXCEPTION;
}

int
js_navigator_init(JSContext *ctx, JSValue global_obj)
{
	JSValue navigator_proto, navigator_class;

	/* create the navigator class */
	JS_NewClassID(&js_navigator_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_navigator_class_id, &js_navigator_class);

	navigator_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, navigator_proto, js_navigator_proto_funcs, countof(js_navigator_proto_funcs));

	navigator_class = JS_NewCFunction2(ctx, js_navigator_ctor, "navigator", 0, JS_CFUNC_constructor, 0);
	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, navigator_class, navigator_proto);
	JS_SetClassProto(ctx, js_navigator_class_id, navigator_proto);

	JS_SetPropertyStr(ctx, global_obj, "navigator", navigator_proto);
	return 0;
}
