/* The quickjs unibar objects implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "dialogs/status.h"
#include "document/view.h"
#include "js/ecmascript.h"
#include "js/mujs.h"
#include "js/mujs/unibar.h"
#include "main/select.h"
#include "session/session.h"
#include "viewer/text/vs.h"

static void
mjs_menubar_get_property_visible(js_State *J)
{
	ELOG
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
	ELOG
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
	ELOG
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
	ELOG
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
	ELOG
	js_pushstring(J, "[menubar object]");
}

static void
mjs_statusbar_toString(js_State *J)
{
	ELOG
	js_pushstring(J, "[statusbar object]");
}

static void
mjs_menubar_init(js_State *J)
{
	ELOG
	js_newobject(J);
	{
		addmethod(J, "menubar.toString", mjs_menubar_toString, 0);
		addproperty(J, "menubar.visible", mjs_menubar_get_property_visible, mjs_menubar_set_property_visible);
	}
	js_defglobal(J, "menubar", JS_DONTENUM);
}

static void
mjs_statusbar_init(js_State *J)
{
	ELOG
	js_newobject(J);
	{
		addmethod(J, "statusbar.toString", mjs_statusbar_toString, 0);
		addproperty(J, "statusbar.visible", mjs_statusbar_get_property_visible, mjs_statusbar_set_property_visible);
	}
	js_defglobal(J, "statusbar", JS_DONTENUM);
}

int
mjs_unibar_init(js_State *J)
{
	ELOG
	mjs_menubar_init(J);
	mjs_statusbar_init(J);

	return 0;
}
