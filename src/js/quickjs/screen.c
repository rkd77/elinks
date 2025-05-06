/* The QuickJS screen object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/view.h"
#include "js/ecmascript.h"
#include "js/quickjs.h"
#include "js/quickjs/screen.h"
#include "session/session.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_screen_class_id;

static JSValue
js_screen_get_property_availHeight(JSContext *ctx, JSValueConst this_val)
{
	ELOG
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
	struct session *ses = doc_view->session;

	if (!ses) {
		return JS_UNDEFINED;
	}

	return JS_NewInt32(ctx, doc_view->box.height * ses->tab->term->cell_height);
}

static JSValue
js_screen_get_property_availWidth(JSContext *ctx, JSValueConst this_val)
{
	ELOG
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
	struct session *ses = doc_view->session;

	if (!ses) {
		return JS_UNDEFINED;
	}

	return JS_NewInt32(ctx, doc_view->box.width * ses->tab->term->cell_width);
}

static JSValue
js_screen_get_property_height(JSContext *ctx, JSValueConst this_val)
{
	ELOG
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

	struct session *ses = doc_view->session;

	if (!ses) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}

	return JS_NewInt32(ctx, ses->tab->term->height * ses->tab->term->cell_height);
}

static JSValue
js_screen_get_property_width(JSContext *ctx, JSValueConst this_val)
{
	ELOG
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

	struct session *ses = doc_view->session;

	if (!ses) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}

	return JS_NewInt32(ctx, ses->tab->term->width * ses->tab->term->cell_width);
}

static JSValue
js_screen_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[screen object]");
}

static const JSCFunctionListEntry js_screen_proto_funcs[] = {
	JS_CGETSET_DEF("availHeight", js_screen_get_property_availHeight, NULL),
	JS_CGETSET_DEF("availWidth", js_screen_get_property_availWidth, NULL),
	JS_CGETSET_DEF("height", js_screen_get_property_height, NULL),
	JS_CGETSET_DEF("width", js_screen_get_property_width, NULL),
	JS_CFUNC_DEF("toString", 0, js_screen_toString)
};

static JSClassDef js_screen_class = {
    "screen",
};

int
js_screen_init(JSContext *ctx)
{
	ELOG
	JSValue screen_proto;

	/* create the screen class */
	JS_NewClassID(&js_screen_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_screen_class_id, &js_screen_class);

	JSValue global_obj = JS_GetGlobalObject(ctx);
	REF_JS(global_obj);

	screen_proto = JS_NewObject(ctx);
	REF_JS(screen_proto);

	JS_SetPropertyFunctionList(ctx, screen_proto, js_screen_proto_funcs, countof(js_screen_proto_funcs));
	JS_SetClassProto(ctx, js_screen_class_id, screen_proto);

	JS_SetPropertyStr(ctx, global_obj, "screen", JS_DupValue(ctx, screen_proto));

	JS_FreeValue(ctx, global_obj);
	REF_JS(global_obj);

	return 0;
}
