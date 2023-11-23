/* The QuickJS history object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/history.h"
#include "ecmascript/mujs/window.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "viewer/text/vs.h"

static void
mjs_history_back(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);

	assert(interpreter);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct session *ses = doc_view->session;

	go_back(ses);

/* history_back() must return 0 for onClick to cause displaying previous page
 * and return non zero for <a href="javascript:history.back()"> to prevent
 * "calculating" new link. Returned value 2 is changed to 0 in function
 * spidermonkey_eval_boolback */
	js_pushnull(J);
}

static void
mjs_history_forward(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);

	assert(interpreter);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct session *ses = doc_view->session;

	go_unback(ses);

	js_pushnull(J);
}

static void
mjs_history_go(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);

	assert(interpreter);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct session *ses = doc_view->session;
	struct location *loc;

	int index = js_tointeger(J, 1);

	for (loc = cur_loc(ses);
	     loc != (struct location *) &ses->history.history;
	     loc = index > 0 ? loc->next : loc->prev) {
		if (!index) {
			go_history(ses, loc);
			break;
		}

		index += index > 0 ? -1 : 1;
	}

	js_pushnull(J);
}

static void
mjs_history_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[history object]");
}

int
mjs_history_init(js_State *J)
{
	js_newobject(J);
	{
		addmethod(J, "history.back", mjs_history_back, 0);
		addmethod(J, "history.forward", mjs_history_forward, 0);
		addmethod(J, "history.go", mjs_history_go, 1);
		addmethod(J, "history.toString", mjs_history_toString, 0);
	}
	js_defglobal(J, "history", JS_DONTENUM);

	return 0;
}
