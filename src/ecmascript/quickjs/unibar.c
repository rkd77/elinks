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
#include "ecmascript/ecmascript.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/unibar.h"
#include "main/select.h"
#include "session/session.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_menubar_class_id;
static JSClassID js_statusbar_class_id;

static JSValue
js_unibar_get_property_visible(JSContext *ctx, JSValueConst this_val, int magic)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

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
	REF_JS(this_val);
	REF_JS(val);

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

static JSValue
js_menubar_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[menubar object]");
}

static JSValue
js_statusbar_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[statusbar object]");
}

static const JSCFunctionListEntry js_menubar_proto_funcs[] = {
	JS_CGETSET_MAGIC_DEF("visible", js_unibar_get_property_visible, js_unibar_set_property_visible, 1),
	JS_CFUNC_DEF("toString", 0, js_menubar_toString)
};

static const JSCFunctionListEntry js_statusbar_proto_funcs[] = {
	JS_CGETSET_MAGIC_DEF("visible", js_unibar_get_property_visible, js_unibar_set_property_visible, 0),
	JS_CFUNC_DEF("toString", 0, js_statusbar_toString)
};

static JSClassDef js_menubar_class = {
	"menubar",
};

static JSClassDef js_statusbar_class = {
	"statusbar",
};

int
js_unibar_init(JSContext *ctx)
{
	JSValue menubar_proto;
	JSValue statusbar_proto;

	/* create the menubar class */
	JS_NewClassID(&js_menubar_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_menubar_class_id, &js_menubar_class);

	JSValue global_obj = JS_GetGlobalObject(ctx);
	REF_JS(global_obj);

	menubar_proto = JS_NewObject(ctx);
	REF_JS(menubar_proto);

	JS_SetPropertyFunctionList(ctx, menubar_proto, js_menubar_proto_funcs, countof(js_menubar_proto_funcs));

	JS_SetClassProto(ctx, js_menubar_class_id, menubar_proto);
	JS_SetPropertyStr(ctx, global_obj, "menubar", JS_DupValue(ctx, menubar_proto));

	/* create the statusbar class */
	JS_NewClassID(&js_statusbar_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_statusbar_class_id, &js_statusbar_class);

	statusbar_proto = JS_NewObject(ctx);
	REF_JS(statusbar_proto);

	JS_SetPropertyFunctionList(ctx, statusbar_proto, js_statusbar_proto_funcs, countof(js_statusbar_proto_funcs));

	JS_SetClassProto(ctx, js_statusbar_class_id, statusbar_proto);
	JS_SetPropertyStr(ctx, global_obj, "statusbar", JS_DupValue(ctx, statusbar_proto));

	JS_FreeValue(ctx, global_obj);

	return 0;
}
