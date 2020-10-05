/* The SpiderMonkey location and history objects implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"

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
#include "ecmascript/spidermonkey/navigator.h"
#include "intl/gettext/libintl.h"
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


static JSBool navigator_get_property_appCodeName(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static JSBool navigator_get_property_appName(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static JSBool navigator_get_property_appVersion(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static JSBool navigator_get_property_language(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static JSBool navigator_get_property_platform(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static JSBool navigator_get_property_userAgent(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);

JSClass navigator_class = {
	"navigator",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub,
	JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL
};

/* Tinyids of properties.  Use negative values to distinguish these
 * from array indexes (even though this object has no array elements).
 * ECMAScript code should not use these directly as in navigator[-1];
 * future versions of ELinks may change the numbers.  */
enum navigator_prop {
	JSP_NAVIGATOR_APP_CODENAME = -1,
	JSP_NAVIGATOR_APP_NAME     = -2,
	JSP_NAVIGATOR_APP_VERSION  = -3, 
	JSP_NAVIGATOR_LANGUAGE     = -4,
	/* JSP_NAVIGATOR_MIME_TYPES = -5, */
	JSP_NAVIGATOR_PLATFORM     = -6,
	/* JSP_NAVIGATOR_PLUGINS   = -7, */
	JSP_NAVIGATOR_USER_AGENT   = -8,
};
JSPropertySpec navigator_props[] = {
	{ "appCodeName",0,	JSPROP_ENUMERATE | JSPROP_READONLY|JSPROP_SHARED, JSOP_WRAPPER(navigator_get_property_appCodeName), JSOP_NULLWRAPPER },
	{ "appName",	0,	JSPROP_ENUMERATE | JSPROP_READONLY|JSPROP_SHARED, JSOP_WRAPPER(navigator_get_property_appName), JSOP_NULLWRAPPER },
	{ "appVersion",	0,	JSPROP_ENUMERATE | JSPROP_READONLY|JSPROP_SHARED, JSOP_WRAPPER(navigator_get_property_appVersion), JSOP_NULLWRAPPER },
	{ "language",	0,	JSPROP_ENUMERATE | JSPROP_READONLY|JSPROP_SHARED, JSOP_WRAPPER(navigator_get_property_language), JSOP_NULLWRAPPER },
	{ "platform",	0,	JSPROP_ENUMERATE | JSPROP_READONLY|JSPROP_SHARED, JSOP_WRAPPER(navigator_get_property_platform), JSOP_NULLWRAPPER },
	{ "userAgent",	0,	JSPROP_ENUMERATE | JSPROP_READONLY|JSPROP_SHARED, JSOP_WRAPPER(navigator_get_property_userAgent), JSOP_NULLWRAPPER },
	{ NULL }
};


/* @navigator_class.getProperty */

static JSBool
navigator_get_property_appCodeName(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS
	(void)obj;

	string_to_jsval(ctx, vp, "Mozilla"); /* More like a constant nowadays. */

	return JS_TRUE;
}

static JSBool
navigator_get_property_appName(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS
	(void)obj;

	string_to_jsval(ctx, vp, "ELinks (roughly compatible with Netscape Navigator, Mozilla and Microsoft Internet Explorer)");

	return JS_TRUE;
}

static JSBool
navigator_get_property_appVersion(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS
	(void)obj;

	string_to_jsval(ctx, vp, VERSION);

	return JS_TRUE;
}

static JSBool
navigator_get_property_language(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS
	(void)obj;

	undef_to_jsval(ctx, vp);
#ifdef CONFIG_NLS
	if (get_opt_bool("protocol.http.accept_ui_language", NULL))
		string_to_jsval(ctx, vp, language_to_iso639(current_language));

#endif
	return JS_TRUE;
}

static JSBool
navigator_get_property_platform(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS
	(void)obj;

	string_to_jsval(ctx, vp, system_name);

	return JS_TRUE;
}

static JSBool
navigator_get_property_userAgent(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS
	unsigned char *optstr;
	(void)obj;

	optstr = get_opt_str("protocol.http.user_agent", NULL);

	if (*optstr && strcmp(optstr, " ")) {
		unsigned char *ustr, ts[64] = "";
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
			string_to_jsval(ctx, vp, custr);

			return JS_TRUE;
		}
	}
	string_to_jsval(ctx, vp, system_name);

	return JS_TRUE;
}
