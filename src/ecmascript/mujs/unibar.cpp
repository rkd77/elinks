/* The quickjs unibar objects implementation. */

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
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/unibar.h"
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

static void
mjs_menubar_get_property_visible(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);

	assert(interpreter);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;

	if (!doc_view) {
		js_pushundefined(J);
		return;
	}
	struct session_status *status = &doc_view->session->status;

	js_pushboolean(J, status->force_show_title_bar >= 0
		? status->force_show_title_bar
		: status->show_title_bar);
}

static void
mjs_statusbar_get_property_visible(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);

	assert(interpreter);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;

	if (!doc_view) {
		js_pushundefined(J);
		return;
	}
	struct session_status *status = &doc_view->session->status;

	js_pushboolean(J, status->force_show_status_bar >= 0
		? status->force_show_status_bar
		: status->show_status_bar);
}

static void
mjs_menubar_set_property_visible(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	int val = js_toboolean(J, 1);

	if (!doc_view) {
		js_pushundefined(J);
		return;
	}
	struct session_status *status = &doc_view->session->status;
	status->force_show_title_bar = val;

	register_bottom_half(update_status, NULL);

	js_pushundefined(J);
}

static void
mjs_statusbar_set_property_visible(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	int val = js_toboolean(J, 1);

	if (!doc_view) {
		js_pushundefined(J);
		return;
	}
	struct session_status *status = &doc_view->session->status;
	status->force_show_status_bar = val;

	register_bottom_half(update_status, NULL);

	js_pushundefined(J);
}

static void
mjs_menubar_toString(js_State *J)
{
	js_pushstring(J, "[menubar object]");
}

static void
mjs_statusbar_toString(js_State *J)
{
	js_pushstring(J, "[statusbar object]");
}

static void
mjs_menubar_init(js_State *J)
{
	js_newobject(J);
	{
		js_newcfunction(J, mjs_menubar_toString, "menubar.toString", 0);
		js_defproperty(J, -2, "toString", JS_DONTENUM);

		js_newcfunction(J, mjs_menubar_get_property_visible, "menubar.visible", 0);
		js_newcfunction(J, mjs_menubar_set_property_visible, "menubar.visible", 1);
		js_defaccessor(J, -3, "visible", JS_DONTENUM | JS_DONTCONF);
	}
	js_defglobal(J, "menubar", JS_DONTENUM);
}

static void
mjs_statusbar_init(js_State *J)
{
	js_newobject(J);
	{
		js_newcfunction(J, mjs_statusbar_toString, "statusbar.toString", 0);
		js_defproperty(J, -2, "toString", JS_DONTENUM);

		js_newcfunction(J, mjs_statusbar_get_property_visible, "statusbar.visible", 0);
		js_newcfunction(J, mjs_statusbar_set_property_visible, "statusbar.visible", 1);
		js_defaccessor(J, -3, "visible", JS_DONTENUM | JS_DONTCONF);
	}
	js_defglobal(J, "statusbar", JS_DONTENUM);
}

int
mjs_unibar_init(js_State *J)
{
	mjs_menubar_init(J);
	mjs_statusbar_init(J);

	return 0;
}
