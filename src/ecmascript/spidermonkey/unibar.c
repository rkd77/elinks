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

static bool unibar_get_property_visible(JSContext *ctx, unsigned int argc, jsval *vp);
static bool unibar_set_property_visible(JSContext *ctx, unsigned int argc, jsval *vp);

/* Each @menubar_class object must have a @window_class parent.  */
JSClass menubar_class = {
	"menubar",
	JSCLASS_HAS_PRIVATE,	/* const char * "t" */
	JS_PropertyStub, nullptr,
	JS_PropertyStub, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr, nullptr
};
/* Each @statusbar_class object must have a @window_class parent.  */
JSClass statusbar_class = {
	"statusbar",
	JSCLASS_HAS_PRIVATE,	/* const char * "s" */
	JS_PropertyStub, nullptr,
	JS_PropertyStub, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr, nullptr
};

/* Tinyids of properties.  Use negative values to distinguish these
 * from array indexes (even though this object has no array elements).
 * ECMAScript code should not use these directly as in menubar[-1];
 * future versions of ELinks may change the numbers.  */
enum unibar_prop {
	JSP_UNIBAR_VISIBLE = -1,
};
JSPropertySpec unibar_props[] = {
	JS_PSGS("visible",	unibar_get_property_visible, unibar_set_property_visible, JSPROP_ENUMERATE),
	{ NULL }
};



static bool
unibar_get_property_visible(JSContext *ctx, unsigned int argc, jsval *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct session_status *status;
	unsigned char *bar;

	/* This can be called if @obj if not itself an instance of either
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &menubar_class, NULL)
	 && !JS_InstanceOf(ctx, hobj, &statusbar_class, NULL))
		return false;

	JS::RootedObject parent_win(ctx, JS_GetParent(hobj));
	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return false;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	status = &doc_view->session->status;
	bar = JS_GetPrivate(hobj); /* from @menubar_class or @statusbar_class */

#define unibar_fetch(bar) \
	status->force_show_##bar##_bar >= 0 \
	          ? status->force_show_##bar##_bar \
	          : status->show_##bar##_bar
	switch (*bar) {
	case 's':
		args.rval().setBoolean(unibar_fetch(status));
		break;
	case 't':
		args.rval().setBoolean(unibar_fetch(title));
		break;
	default:
		args.rval().setBoolean(false);
		break;
	}
#undef unibar_fetch

	return true;
}

static bool
unibar_set_property_visible(JSContext *ctx, unsigned int argc, jsval *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct session_status *status;
	unsigned char *bar;

	/* This can be called if @obj if not itself an instance of either
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &menubar_class, NULL)
	 && !JS_InstanceOf(ctx, hobj, &statusbar_class, NULL))
		return false;

	JS::RootedObject parent_win(ctx, JS_GetParent(hobj));
	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return false;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	status = &doc_view->session->status;
	bar = JS_GetPrivate(hobj); /* from @menubar_class or @statusbar_class */

	switch (*bar) {
	case 's':
		status->force_show_status_bar = args[0].toBoolean();
		break;
	case 't':
		status->force_show_title_bar = args[0].toBoolean();
		break;
	default:
		break;
	}
	register_bottom_half(update_status, NULL);

	return true;
}
