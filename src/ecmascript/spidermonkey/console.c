/* The SpiderMonkey console object implementation. */

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
#include "config/home.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey/console.h"
#include "ecmascript/spidermonkey/form.h"
#include "ecmascript/spidermonkey/location.h"
#include "ecmascript/spidermonkey/document.h"
#include "ecmascript/spidermonkey/window.h"
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

#include <time.h>
#include "document/renderer.h"
#include "document/refresh.h"
#include "terminal/screen.h"

#define DEBUG 0

static bool console_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);

JSClassOps console_ops = {
	JS_PropertyStub, nullptr,
	console_get_property, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr
};

/* Each @console_class object must have a @window_class parent.  */
const JSClass console_class = {
	"console",
	JSCLASS_HAS_PRIVATE,
	&console_ops
};

/* Tinyids of properties.  Use negative values to distinguish these
 * from array indexes (even though this object has no array elements).
 * ECMAScript code should not use these directly as in document[-1];
 * future versions of ELinks may change the numbers.  */
enum console_prop {
	JSP_LOCALSTORAGE_LOC   = -1,
	JSP_LOCALSTORAGE_REF   = -2,
	JSP_LOCALSTORAGE_TITLE = -3,
	JSP_LOCALSTORAGE_URL   = -4,
};
/* "cookie" is special; it isn't a regular property but we channel it to the
 * cookie-module. XXX: Would it work if "cookie" was defined in this array? */
const JSPropertySpec console_props[] = {
	{ NULL }
};

/* @console_class.getProperty */
static bool
console_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;

	return true;
}

static bool console_log(JSContext *ctx, unsigned int argc, JS::Value *vp);

const spidermonkeyFunctionSpec console_funcs[] = {
	{ "log",		console_log,	 	2 },
	{ NULL }
};

static bool
console_log(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	if (argc != 1) 
	{
		args.rval().setBoolean(false);
		return(true);
	}

	unsigned char *key= JS_EncodeString(ctx, args[0].toString());

	char *log_fname = stracpy(elinks_home);
	add_to_strn(&log_fname,"console.log");

	FILE *f = fopen(log_fname,"a");
	if (f) 
	{
		fprintf(f, "%s\n", key);
		fclose(f);
	}

	args.rval().setBoolean(true);
	return(true);

}
