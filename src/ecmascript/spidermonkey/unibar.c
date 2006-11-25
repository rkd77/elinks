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
#include "ecmascript/spidermonkey/unibar.h"
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


static JSBool unibar_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static JSBool unibar_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

const JSClass menubar_class = {
	"menubar",
	JSCLASS_HAS_PRIVATE,	/* const char * "t" */
	JS_PropertyStub, JS_PropertyStub,
	unibar_get_property, unibar_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};
const JSClass statusbar_class = {
	"statusbar",
	JSCLASS_HAS_PRIVATE,	/* const char * "s" */
	JS_PropertyStub, JS_PropertyStub,
	unibar_get_property, unibar_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

enum unibar_prop { JSP_UNIBAR_VISIBLE };
const JSPropertySpec unibar_props[] = {
	{ "visible",	JSP_UNIBAR_VISIBLE,	JSPROP_ENUMERATE },
	{ NULL }
};


static JSBool
unibar_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent = JS_GetParent(ctx, obj);
	struct view_state *vs = JS_GetPrivate(ctx, parent);
	struct document_view *doc_view = vs->doc_view;
	struct session_status *status = &doc_view->session->status;
	unsigned char *bar = JS_GetPrivate(ctx, obj);

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_UNIBAR_VISIBLE:
#define unibar_fetch(bar) \
	boolean_to_jsval(ctx, vp, status->force_show_##bar##_bar >= 0 \
	          ? status->force_show_##bar##_bar \
	          : status->show_##bar##_bar)
		switch (*bar) {
		case 's':
			unibar_fetch(status);
			break;
		case 't':
			unibar_fetch(title);
			break;
		default:
			boolean_to_jsval(ctx, vp, 0);
			break;
		}
#undef unibar_fetch
		break;
	default:
		INTERNAL("Invalid ID %d in unibar_get_property().", JSVAL_TO_INT(id));
		break;
	}

	return JS_TRUE;
}

static JSBool
unibar_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent = JS_GetParent(ctx, obj);
	struct view_state *vs = JS_GetPrivate(ctx, parent);
	struct document_view *doc_view = vs->doc_view;
	struct session_status *status = &doc_view->session->status;
	unsigned char *bar = JS_GetPrivate(ctx, obj);

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_UNIBAR_VISIBLE:
		switch (*bar) {
		case 's':
			status->force_show_status_bar = jsval_to_boolean(ctx, vp);
			break;
		case 't':
			status->force_show_title_bar = jsval_to_boolean(ctx, vp);
			break;
		default:
			break;
		}
		register_bottom_half(update_status, NULL);
		break;
	default:
		INTERNAL("Invalid ID %d in unibar_set_property().", JSVAL_TO_INT(id));
		return JS_TRUE;
	}

	return JS_TRUE;
}
