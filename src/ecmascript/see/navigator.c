/* The SEE navigator objects implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include <see/see.h>

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
#include "ecmascript/see/input.h"
#include "ecmascript/see/navigator.h"
#include "ecmascript/see/strings.h"
#include "ecmascript/see/window.h"
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

static void navigator_get(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *, struct SEE_value *);
static int navigator_hasproperty(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *);

struct SEE_objectclass js_navigator_object_class = {
	"navigator",
	navigator_get,
	SEE_no_put,
	SEE_no_canput,
	navigator_hasproperty,
	SEE_no_delete,
	SEE_no_defaultvalue,
	SEE_no_enumerator,
	NULL,
	NULL,
	NULL
};

static void
navigator_get(struct SEE_interpreter *interp, struct SEE_object *o,
	   struct SEE_string *p, struct SEE_value *res)
{
	struct SEE_string *str;

	SEE_SET_UNDEFINED(res);
	if (p == s_appCodeName) {
		SEE_SET_STRING(res, s_Mozilla);
	} else if (p == s_appName) {
		SEE_SET_STRING(res, s_ELinks_);
	} else if (p == s_appVersion) {
		str = string_to_SEE_string(interp, VERSION);
		SEE_SET_STRING(res, str);
	} else if (p == s_language) {
#ifdef CONFIG_NLS
		if (get_opt_bool("protocol.http.accept_ui_language")) {
			str = string_to_SEE_string(interp,
			 language_to_iso639(current_language));
			SEE_SET_STRING(res, str);
		}
#endif
	} else if (p == s_platform) {
		str = string_to_SEE_string(interp, system_name);
		SEE_SET_STRING(res, str);
	} else if (p == s_userAgent) {
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
				str = string_to_SEE_string(interp, custr);
				SEE_SET_STRING(res, str);
			}
		}
	}
}


static int
navigator_hasproperty(struct SEE_interpreter *interp, struct SEE_object *o,
	      struct SEE_string *p)
{
	if (p == s_appCodeName || p == s_appName || p == s_appVersion
	    || p == s_language || p == s_platform || p == s_userAgent)
		return 1;
	return 0;
}



void
init_js_navigator_object(struct ecmascript_interpreter *interpreter)
{
	struct global_object *g = interpreter->backend_data;
	struct SEE_interpreter *interp = &g->interp;
	struct SEE_value v;
	struct SEE_object *navigator;

	navigator = SEE_NEW(interp, struct SEE_object);

	navigator->objectclass = &js_navigator_object_class;
	navigator->Prototype = NULL;

	SEE_SET_OBJECT(&v, navigator);
	SEE_OBJECT_PUT(interp, interp->Global, s_navigator, &v, 0);
}
