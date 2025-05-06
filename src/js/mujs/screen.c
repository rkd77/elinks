/* The MuJS screen object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/view.h"
#include "js/ecmascript.h"
#include "js/mujs.h"
#include "js/mujs/screen.h"
#include "js/mujs/window.h"
#include "session/session.h"
#include "viewer/text/vs.h"

static void
mjs_screen_get_property_availHeight(js_State *J)
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
	struct session *ses = doc_view->session;

	if (!ses) {
		js_pushundefined(J);
		return;
	}
	js_pushnumber(J, doc_view->box.height * ses->tab->term->cell_height);
}

static void
mjs_screen_get_property_availWidth(js_State *J)
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
	struct session *ses = doc_view->session;

	if (!ses) {
		js_pushundefined(J);
		return;
	}
	js_pushnumber(J, doc_view->box.width * ses->tab->term->cell_width);
}

static void
mjs_screen_get_property_height(js_State *J)
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
	struct session *ses = doc_view->session;

	if (!ses) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushundefined(J);
		return;
	}
	js_pushnumber(J, ses->tab->term->height * ses->tab->term->cell_height);
}

static void
mjs_screen_get_property_width(js_State *J)
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
	struct session *ses = doc_view->session;

	if (!ses) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushundefined(J);
		return;
	}
	js_pushnumber(J, ses->tab->term->width * ses->tab->term->cell_width);
}

static void
mjs_screen_toString(js_State *J)
{
	ELOG
	js_pushstring(J, "[screen object]");
}

int
mjs_screen_init(js_State *J)
{
	ELOG
	js_newobject(J);
	{
		addmethod(J, "screen.toString", mjs_screen_toString, 0);
		addproperty(J, "screen.availHeight", mjs_screen_get_property_availHeight, NULL);
		addproperty(J, "screen.availWidth", mjs_screen_get_property_availWidth, NULL);
		addproperty(J, "screen.height", mjs_screen_get_property_height, NULL);
		addproperty(J, "screen.width", mjs_screen_get_property_width, NULL);
	}
	js_defglobal(J, "screen", JS_DONTENUM);

	return 0;
}
