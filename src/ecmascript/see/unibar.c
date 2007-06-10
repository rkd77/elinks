/* The SEE location and history objects implementation. */

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
#include "ecmascript/see/strings.h"
#include "ecmascript/see/unibar.h"
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

static void unibar_get(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *, struct SEE_value *);
static void unibar_put(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *, struct SEE_value *, int);
static int unibar_canput(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *);
static int unibar_hasproperty(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *);

struct js_unibar_object {
	struct SEE_object object;
	unsigned char bar;
};

struct SEE_objectclass js_menubar_object_class = {
	"menubar",
	unibar_get,
	unibar_put,
	unibar_canput,
	unibar_hasproperty,
	SEE_no_delete,
	SEE_no_defaultvalue,
	SEE_no_enumerator,
	NULL,
	NULL,
	NULL
};

struct SEE_objectclass js_statusbar_object_class = {
	"statusbar",
	unibar_get,
	unibar_put,
	unibar_canput,
	unibar_hasproperty,
	SEE_no_delete,
	SEE_no_defaultvalue,
	SEE_no_enumerator,
	NULL,
	NULL,
	NULL
};

static void
unibar_get(struct SEE_interpreter *interp, struct SEE_object *o,
	   struct SEE_string *p, struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct session_status *status = &doc_view->session->status;
	struct js_unibar_object *obj = (struct js_unibar_object *)o;
	unsigned char bar = obj->bar;

	if (p == s_visible) {
#define unibar_fetch(bar) \
	SEE_SET_BOOLEAN(res, status->force_show_##bar##_bar >= 0 \
	          ? status->force_show_##bar##_bar \
	          : status->show_##bar##_bar)
		switch (bar) {
			case 's':
				unibar_fetch(status);
				break;
			case 't':
				unibar_fetch(title);
				break;
			default:
				SEE_SET_BOOLEAN(res, 0);
				break;
		}
#undef unibar_fetch
		return;
	}
	SEE_SET_UNDEFINED(res);
}

static void
unibar_put(struct SEE_interpreter *interp, struct SEE_object *o,
	   struct SEE_string *p, struct SEE_value *val, int attr)
{
	if (p == s_visible) {
		struct global_object *g = (struct global_object *)interp;
		struct view_state *vs = g->win->vs;
		struct document_view *doc_view = vs->doc_view;
		struct session_status *status = &doc_view->session->status;
		struct js_unibar_object *obj = (struct js_unibar_object *)o;
		unsigned char bar = obj->bar;

		switch (bar) {
			case 's':
				status->force_show_status_bar =
				 SEE_ToUint32(interp, val);
				break;
			case 't':
				status->force_show_title_bar =
				 SEE_ToUint32(interp, val);
				break;
			default:
				break;

		}
	}
}

static int
unibar_canput(struct SEE_interpreter *interp, struct SEE_object *o,
	      struct SEE_string *p)
{
	if (p == s_visible)
		return 1;
	return 0;
}

static int
unibar_hasproperty(struct SEE_interpreter *interp, struct SEE_object *o,
	      struct SEE_string *p)
{
	if (p == s_visible)
		return 1;
	return 0;
}

void
init_js_menubar_object(struct ecmascript_interpreter *interpreter)
{
	struct global_object *g = interpreter->backend_data;
	struct SEE_interpreter *interp = &g->interp;
	struct SEE_value v;
	struct js_unibar_object *menu;

	menu = SEE_NEW(interp, struct js_unibar_object);

	menu->object.objectclass = &js_menubar_object_class;
	menu->object.Prototype = NULL;
	menu->bar = 't';

	SEE_SET_OBJECT(&v, (struct SEE_object *)menu);
	SEE_OBJECT_PUT(interp, interp->Global, s_menubar, &v, 0);
}

void
init_js_statusbar_object(struct ecmascript_interpreter *interpreter)
{
	struct global_object *g = interpreter->backend_data;
	struct SEE_interpreter *interp = &g->interp;
	struct SEE_value v;
	struct js_unibar_object *status;

	status = SEE_NEW(interp, struct js_unibar_object);

	status->object.objectclass = &js_statusbar_object_class;
	status->object.Prototype = NULL;
	status->bar = 's';

	SEE_SET_OBJECT(&v, (struct SEE_object *)status);
	SEE_OBJECT_PUT(interp, interp->Global, s_statusbar, &v, 0);
}
