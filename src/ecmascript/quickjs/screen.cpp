/* The QuickJS screen object implementation. */

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
#include "ecmascript/quickjs/screen.h"
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

static JSClassID js_screen_class_id;

static JSValue
js_screen_get_property_availHeight(JSContext *ctx, JSValueConst this_val)
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

	return JS_NewInt32(ctx, doc_view->box.height * 16);
}

static JSValue
js_screen_get_property_availWidth(JSContext *ctx, JSValueConst this_val)
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

	return JS_NewInt32(ctx, doc_view->box.width * 8);
}

static JSValue
js_screen_get_property_height(JSContext *ctx, JSValueConst this_val)
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

	struct session *ses = doc_view->session;

	if (!ses) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}

	return JS_NewInt32(ctx, ses->tab->term->height * 16);
}

static JSValue
js_screen_get_property_width(JSContext *ctx, JSValueConst this_val)
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

	struct session *ses = doc_view->session;

	if (!ses) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}

	return JS_NewInt32(ctx, ses->tab->term->width * 8);
}

static JSValue
js_screen_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return JS_NewString(ctx, "[screen object]");
}

static const JSCFunctionListEntry js_screen_proto_funcs[] = {
	JS_CGETSET_DEF("availHeight", js_screen_get_property_availHeight, nullptr),
	JS_CGETSET_DEF("availWidth", js_screen_get_property_availWidth, nullptr),
	JS_CGETSET_DEF("height", js_screen_get_property_height, nullptr),
	JS_CGETSET_DEF("width", js_screen_get_property_width, nullptr),
	JS_CFUNC_DEF("toString", 0, js_screen_toString)
};

static JSClassDef js_screen_class = {
    "screen",
};

static JSValue
js_screen_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_screen_class_id);
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
js_screen_init(JSContext *ctx)
{
	JSValue screen_proto, screen_class;

	/* create the screen class */
	JS_NewClassID(&js_screen_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_screen_class_id, &js_screen_class);

	JSValue global_obj = JS_GetGlobalObject(ctx);

	screen_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, screen_proto, js_screen_proto_funcs, countof(js_screen_proto_funcs));

	screen_class = JS_NewCFunction2(ctx, js_screen_ctor, "screen", 2, JS_CFUNC_constructor, 0);
	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, screen_class, screen_proto);
	JS_SetClassProto(ctx, js_screen_class_id, screen_proto);

	JS_SetPropertyStr(ctx, global_obj, "screen", screen_proto);

	JS_FreeValue(ctx, global_obj);

	return 0;
}
