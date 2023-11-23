/* The QuickJS KeyboardEvent object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/ecmascript.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/keyboard.h"
#include "intl/charsets.h"
#include "terminal/event.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_keyboardEvent_class_id;

static JSValue js_keyboardEvent_get_property_key(JSContext *ctx, JSValueConst this_val);
static JSValue js_keyboardEvent_get_property_keyCode(JSContext *ctx, JSValueConst this_val);

static unicode_val_T keyCode;

struct keyboard {
	unicode_val_T keyCode;
};

static
void js_keyboardEvent_finalizer(JSRuntime *rt, JSValue val)
{
	REF_JS(val);

	struct keyboard *keyb = (struct keyboard *)JS_GetOpaque(val, js_keyboardEvent_class_id);

	if (keyb) {
		mem_free(keyb);
	}
}

static JSClassDef js_keyboardEvent_class = {
	"KeyboardEvent",
	js_keyboardEvent_finalizer
};

static const JSCFunctionListEntry js_keyboardEvent_proto_funcs[] = {
	JS_CGETSET_DEF("key",	js_keyboardEvent_get_property_key, NULL),
	JS_CGETSET_DEF("keyCode",	js_keyboardEvent_get_property_keyCode, NULL)
};

static JSValue
js_keyboardEvent_get_property_key(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct keyboard *keyb = (struct keyboard *)(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

	if (!keyb) {
		return JS_NULL;
	}
	char text[8] = {0};

	*text = keyb->keyCode;
	JSValue r = JS_NewString(ctx, text);

	RETURN_JS(r);
}

static JSValue
js_keyboardEvent_get_property_keyCode(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct keyboard *keyb = (struct keyboard *)(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

	if (!keyb) {
		return JS_NULL;
	}
	return JS_NewUint32(ctx, keyb->keyCode);
}

JSValue
get_keyboardEvent(JSContext *ctx, struct term_event *ev)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	static int initialized;
	/* create the element class */
	if (!initialized) {
		JS_NewClassID(&js_keyboardEvent_class_id);
		JS_NewClass(JS_GetRuntime(ctx), js_keyboardEvent_class_id, &js_keyboardEvent_class);
		initialized = 1;
	}
	struct keyboard *keyb = (struct keyboard *)mem_calloc(1, sizeof(*keyb));

	if (!keyb) {
		return JS_NULL;
	}
	keyCode = get_kbd_key(ev);

	if (keyCode == KBD_ENTER) {
		keyCode = 13;
	}
	keyb->keyCode = keyCode;

	JSValue keyb_obj = JS_NewObjectClass(ctx, js_keyboardEvent_class_id);
	JS_SetPropertyFunctionList(ctx, keyb_obj, js_keyboardEvent_proto_funcs, countof(js_keyboardEvent_proto_funcs));
	JS_SetClassProto(ctx, js_keyboardEvent_class_id, keyb_obj);
	JS_SetOpaque(keyb_obj, keyb);

	JSValue rr = JS_DupValue(ctx, keyb_obj);
	RETURN_JS(rr);
}
