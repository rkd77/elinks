/* The QuickJS DOMRect implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/ecmascript.h"
#include "js/libdom/dom.h"
#include "js/quickjs.h"
#include "js/quickjs/element.h"
#include "js/quickjs/event.h"
#include "intl/charsets.h"
#include "terminal/event.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

JSClassID js_domRect_class_id;

struct eljs_domrect {
	double x;
	double y;
	double width;
	double height;
	double top;
	double right;
	double bottom;
	double left;
};

static JSValue js_domRect_get_property_bottom(JSContext *ctx, JSValueConst this_val);
static JSValue js_domRect_get_property_height(JSContext *ctx, JSValueConst this_val);
static JSValue js_domRect_get_property_left(JSContext *ctx, JSValueConst this_val);
static JSValue js_domRect_get_property_right(JSContext *ctx, JSValueConst this_val);
static JSValue js_domRect_get_property_top(JSContext *ctx, JSValueConst this_val);
static JSValue js_domRect_get_property_width(JSContext *ctx, JSValueConst this_val);
static JSValue js_domRect_get_property_x(JSContext *ctx, JSValueConst this_val);
static JSValue js_domRect_get_property_y(JSContext *ctx, JSValueConst this_val);

static JSValue js_domRect_set_property_bottom(JSContext *ctx, JSValueConst this_val, JSValue val);
static JSValue js_domRect_set_property_height(JSContext *ctx, JSValueConst this_val, JSValue val);
static JSValue js_domRect_set_property_left(JSContext *ctx, JSValueConst this_val, JSValue val);
static JSValue js_domRect_set_property_right(JSContext *ctx, JSValueConst this_val, JSValue val);
static JSValue js_domRect_set_property_top(JSContext *ctx, JSValueConst this_val, JSValue val);
static JSValue js_domRect_set_property_width(JSContext *ctx, JSValueConst this_val, JSValue val);
static JSValue js_domRect_set_property_x(JSContext *ctx, JSValueConst this_val, JSValue val);
static JSValue js_domRect_set_property_y(JSContext *ctx, JSValueConst this_val, JSValue val);

static
void js_domRect_finalizer(JSRuntime *rt, JSValue val)
{
	REF_JS(val);

	struct eljs_domrect *d = (struct eljs_domrect *)JS_GetOpaque(val, js_domRect_class_id);

	if (d) {
		mem_free(d);
	}
}

static JSClassDef js_domRect_class = {
	"DOMRect",
	js_domRect_finalizer
};

static const JSCFunctionListEntry js_domRect_proto_funcs[] = {
	JS_CGETSET_DEF("bottom",	js_domRect_get_property_bottom, js_domRect_set_property_bottom),
	JS_CGETSET_DEF("height",	js_domRect_get_property_height, js_domRect_set_property_height),
	JS_CGETSET_DEF("left",	js_domRect_get_property_left, js_domRect_set_property_left),
	JS_CGETSET_DEF("right",	js_domRect_get_property_right, js_domRect_set_property_right),
	JS_CGETSET_DEF("top",	js_domRect_get_property_top, js_domRect_set_property_top),
	JS_CGETSET_DEF("width",	js_domRect_get_property_width, js_domRect_set_property_width),
	JS_CGETSET_DEF("x",	js_domRect_get_property_x, js_domRect_set_property_x),
	JS_CGETSET_DEF("y",	js_domRect_get_property_y, js_domRect_set_property_y),
};

static JSValue
js_domRect_get_property_bottom(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_domrect *d = (struct eljs_domrect *)(JS_GetOpaque(this_val, js_domRect_class_id));

	if (!d) {
		return JS_NULL;
	}
	JSValue r = JS_NewFloat64(ctx, d->bottom);

	RETURN_JS(r);
}

static JSValue
js_domRect_get_property_height(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_domrect *d = (struct eljs_domrect *)(JS_GetOpaque(this_val, js_domRect_class_id));

	if (!d) {
		return JS_NULL;
	}
	JSValue r = JS_NewFloat64(ctx, d->height);

	RETURN_JS(r);
}

static JSValue
js_domRect_get_property_left(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_domrect *d = (struct eljs_domrect *)(JS_GetOpaque(this_val, js_domRect_class_id));

	if (!d) {
		return JS_NULL;
	}
	JSValue r = JS_NewFloat64(ctx, d->left);

	RETURN_JS(r);
}

static JSValue
js_domRect_get_property_right(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_domrect *d = (struct eljs_domrect *)(JS_GetOpaque(this_val, js_domRect_class_id));

	if (!d) {
		return JS_NULL;
	}
	JSValue r = JS_NewFloat64(ctx, d->right);

	RETURN_JS(r);
}

static JSValue
js_domRect_get_property_top(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_domrect *d = (struct eljs_domrect *)(JS_GetOpaque(this_val, js_domRect_class_id));

	if (!d) {
		return JS_NULL;
	}
	JSValue r = JS_NewFloat64(ctx, d->top);

	RETURN_JS(r);
}

static JSValue
js_domRect_get_property_width(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_domrect *d = (struct eljs_domrect *)(JS_GetOpaque(this_val, js_domRect_class_id));

	if (!d) {
		return JS_NULL;
	}
	JSValue r = JS_NewFloat64(ctx, d->width);

	RETURN_JS(r);
}

static JSValue
js_domRect_get_property_x(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_domrect *d = (struct eljs_domrect *)(JS_GetOpaque(this_val, js_domRect_class_id));

	if (!d) {
		return JS_NULL;
	}
	JSValue r = JS_NewFloat64(ctx, d->x);

	RETURN_JS(r);
}

static JSValue
js_domRect_get_property_y(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_domrect *d = (struct eljs_domrect *)(JS_GetOpaque(this_val, js_domRect_class_id));

	if (!d) {
		return JS_NULL;
	}
	JSValue r = JS_NewFloat64(ctx, d->y);

	RETURN_JS(r);
}

static JSValue
js_domRect_set_property_bottom(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_domrect *d = (struct eljs_domrect *)(JS_GetOpaque(this_val, js_domRect_class_id));

	if (!d) {
		return JS_NULL;
	}
	JS_ToFloat64(ctx, &d->bottom, val);

	return JS_UNDEFINED;
}

static JSValue
js_domRect_set_property_height(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_domrect *d = (struct eljs_domrect *)(JS_GetOpaque(this_val, js_domRect_class_id));

	if (!d) {
		return JS_NULL;
	}
	JS_ToFloat64(ctx, &d->height, val);

	return JS_UNDEFINED;
}

static JSValue
js_domRect_set_property_left(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_domrect *d = (struct eljs_domrect *)(JS_GetOpaque(this_val, js_domRect_class_id));

	if (!d) {
		return JS_NULL;
	}
	JS_ToFloat64(ctx, &d->left, val);

	return JS_UNDEFINED;
}

static JSValue
js_domRect_set_property_right(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_domrect *d = (struct eljs_domrect *)(JS_GetOpaque(this_val, js_domRect_class_id));

	if (!d) {
		return JS_NULL;
	}
	JS_ToFloat64(ctx, &d->right, val);

	return JS_UNDEFINED;
}

static JSValue
js_domRect_set_property_top(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_domrect *d = (struct eljs_domrect *)(JS_GetOpaque(this_val, js_domRect_class_id));

	if (!d) {
		return JS_NULL;
	}
	JS_ToFloat64(ctx, &d->top, val);

	return JS_UNDEFINED;
}

static JSValue
js_domRect_set_property_width(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_domrect *d = (struct eljs_domrect *)(JS_GetOpaque(this_val, js_domRect_class_id));

	if (!d) {
		return JS_NULL;
	}
	JS_ToFloat64(ctx, &d->width, val);

	return JS_UNDEFINED;
}

static JSValue
js_domRect_set_property_x(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_domrect *d = (struct eljs_domrect *)(JS_GetOpaque(this_val, js_domRect_class_id));

	if (!d) {
		return JS_NULL;
	}
	JS_ToFloat64(ctx, &d->x, val);

	return JS_UNDEFINED;
}

static JSValue
js_domRect_set_property_y(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_domrect *d = (struct eljs_domrect *)(JS_GetOpaque(this_val, js_domRect_class_id));

	if (!d) {
		return JS_NULL;
	}
	JS_ToFloat64(ctx, &d->y, val);

	return JS_UNDEFINED;
}

JSValue
getDomRect(JSContext *ctx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_domrect *d = mem_calloc(1, sizeof(*d));

	JSValue proto;

	{
		static int initialised;

		if (!initialised) {
			/* Event class */
			JS_NewClassID(&js_domRect_class_id);
			JS_NewClass(JS_GetRuntime(ctx), js_domRect_class_id, &js_domRect_class);
			initialised = 1;
		}
	}
	proto = JS_NewObject(ctx);
	REF_JS(proto);

	JS_SetPropertyFunctionList(ctx, proto, js_domRect_proto_funcs, countof(js_domRect_proto_funcs));
	JS_SetClassProto(ctx, js_domRect_class_id, proto);

//	JSValue obj = JS_NewObjectClass(ctx, js_domRect_class_id);
//	REF_JS(obj);

	JS_SetOpaque(proto, d);
	JSValue r = proto;

	JS_DupValue(ctx, r);

	RETURN_JS(r);
}
