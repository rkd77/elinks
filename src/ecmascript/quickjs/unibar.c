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
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/unibar.h"
#include "ecmascript/quickjs/window.h"
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

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_menubar_class_id;
static JSClassID js_statusbar_class_id;

static JSValue
js_unibar_get_property_visible(JSContext *ctx, JSValueConst this_val, int magic)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;

	if (!doc_view) {
		return JS_UNDEFINED;
	}

	struct session_status *status = &doc_view->session->status;

#define unibar_fetch(bar) \
	status->force_show_##bar##_bar >= 0 \
	          ? status->force_show_##bar##_bar \
	          : status->show_##bar##_bar
	switch (magic) {
	case 0:
		return JS_NewBool(ctx, unibar_fetch(status));
	case 1:
		return JS_NewBool(ctx, unibar_fetch(title));
	default:
		return JS_NewBool(ctx, 0);
	}
#undef unibar_fetch
}

static JSValue
js_unibar_set_property_visible(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;

	if (!doc_view) {
		return JS_UNDEFINED;
	}

	struct session_status *status = &doc_view->session->status;

	switch (magic) {
	case 0:
		status->force_show_status_bar = JS_ToBool(ctx, val);
		break;
	case 1:
		status->force_show_title_bar = JS_ToBool(ctx, val);
		break;
	default:
		break;
	}
	register_bottom_half(update_status, NULL);

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_menubar_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("visible", js_unibar_get_property_visible, js_unibar_set_property_visible, 1),
};

static const JSCFunctionListEntry js_statusbar_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("visible", js_unibar_get_property_visible, js_unibar_set_property_visible, 0),
};

static JSClassDef js_menubar_class = {
    "menubar",
};

static JSClassDef js_statusbar_class = {
    "statusbar",
};

static JSValue
js_menubar_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_menubar_class_id);
	JS_FreeValue(ctx, proto);

	if (JS_IsException(obj)) {
		goto fail;
	}
	RETURN_JS(obj);

fail:
	JS_FreeValue(ctx, obj);
	return JS_EXCEPTION;
}

static JSValue
js_statusbar_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_statusbar_class_id);
	JS_FreeValue(ctx, proto);

	if (JS_IsException(obj)) {
		goto fail;
	}
	RETURN_JS(obj);

fail:
	JS_FreeValue(ctx, obj);
	return JS_EXCEPTION;
}

int
js_unibar_init(JSContext *ctx, JSValue global_obj)
{
	JSValue menubar_proto, menubar_class;
	JSValue statusbar_proto, statusbar_class;

	/* create the menubar class */
	JS_NewClassID(&js_menubar_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_menubar_class_id, &js_menubar_class);
	menubar_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, menubar_proto, js_menubar_proto_funcs, countof(js_menubar_proto_funcs));
	menubar_class = JS_NewCFunction2(ctx, js_menubar_ctor, "menubar", 2, JS_CFUNC_constructor, 0);
	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, menubar_class, menubar_proto);
	JS_SetClassProto(ctx, js_menubar_class_id, menubar_proto);
	JS_SetPropertyStr(ctx, global_obj, "menubar", menubar_proto);

	/* create the statusbar class */
	JS_NewClassID(&js_statusbar_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_statusbar_class_id, &js_statusbar_class);

	statusbar_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, statusbar_proto, js_statusbar_proto_funcs, countof(js_statusbar_proto_funcs));
	statusbar_class = JS_NewCFunction2(ctx, js_statusbar_ctor, "statusbar", 2, JS_CFUNC_constructor, 0);
	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, statusbar_class, statusbar_proto);
	JS_SetClassProto(ctx, js_statusbar_class_id, statusbar_proto);
	JS_SetPropertyStr(ctx, global_obj, "statusbar", statusbar_proto);

	return 0;
}
