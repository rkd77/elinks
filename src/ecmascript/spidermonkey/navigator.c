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


static JSBool navigator_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

const JSClass navigator_class = {
	"navigator",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	navigator_get_property, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
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
const JSPropertySpec navigator_props[] = {
	{ "appCodeName",	JSP_NAVIGATOR_APP_CODENAME,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "appName",		JSP_NAVIGATOR_APP_NAME,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "appVersion",		JSP_NAVIGATOR_APP_VERSION,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "language",		JSP_NAVIGATOR_LANGUAGE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "platform",		JSP_NAVIGATOR_PLATFORM,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "userAgent",		JSP_NAVIGATOR_USER_AGENT,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};


/* @navigator_class.getProperty */
static JSBool
navigator_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	undef_to_jsval(ctx, vp);

	switch (JSVAL_TO_INT(id)) {
	case JSP_NAVIGATOR_APP_CODENAME:
		string_to_jsval(ctx, vp, "Mozilla"); /* More like a constant nowadays. */
		break;
	case JSP_NAVIGATOR_APP_NAME:
		/* This evil hack makes the compatibility checking .indexOf()
		 * code find what it's looking for. */
		string_to_jsval(ctx, vp, "ELinks (roughly compatible with Netscape Navigator, Mozilla and Microsoft Internet Explorer)");
		break;
	case JSP_NAVIGATOR_APP_VERSION:
		string_to_jsval(ctx, vp, VERSION);
		break;
	case JSP_NAVIGATOR_LANGUAGE:
#ifdef CONFIG_NLS
		if (get_opt_bool("protocol.http.accept_ui_language"))
			string_to_jsval(ctx, vp, language_to_iso639(current_language));

#endif
		break;
	case JSP_NAVIGATOR_PLATFORM:
		string_to_jsval(ctx, vp, system_name);
		break;
	case JSP_NAVIGATOR_USER_AGENT:
	{
		/* FIXME: Code duplication. */
		unsigned char *optstr = get_opt_str("protocol.http.user_agent");

		if (*optstr && strcmp(optstr, " ")) {
			unsigned char *ustr, ts[64] = "";
			static unsigned char custr[256];

			if (!list_empty(terminals)) {
				unsigned int tslen = 0;
				struct terminal *term = terminals.prev;

				ulongcat(ts, &tslen, term->width, 3, 0);
				ts[tslen++] = 'x';
				ulongcat(ts, &tslen, term->height, 3, 0);
			}
			ustr = subst_user_agent(optstr, VERSION_STRING, system_name, ts);

			if (ustr) {
				safe_strncpy(custr, ustr, 256);
				mem_free(ustr);
				string_to_jsval(ctx, vp, custr);
			}
		}
	}
		break;
	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return JS_TRUE in this case
		 * and leave *@vp unchanged.  Do the same here.
		 * (Actually not quite the same, as we already used
		 * @undef_to_jsval.)  */
		break;
	}

	return JS_TRUE;
}
