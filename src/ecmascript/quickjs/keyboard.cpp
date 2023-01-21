/* The QuickJS KeyboardEvent object implementation. */

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
#include "ecmascript/quickjs/heartbeat.h"
#include "ecmascript/quickjs/keyboard.h"
#include "ecmascript/timer.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "main/timer.h"
#include "network/connection.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "session/download.h"
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

#include <iostream>
#include <list>
#include <map>
#include <utility>
#include <sstream>
#include <vector>

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

	struct keyboard *keyb = JS_GetOpaque(val, js_keyboardEvent_class_id);

	if (keyb) {
		mem_free(keyb);
	}
}

static JSClassDef js_keyboardEvent_class = {
	"KeyboardEvent",
	js_keyboardEvent_finalizer
};

static JSValue
js_keyboardEvent_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	REF_JS(new_target);

	JSValue obj = JS_UNDEFINED;
	JSValue proto;

	struct keyboard *keyb = (struct keyboard *)mem_calloc(1, sizeof(*keyb));

	if (!keyb) {
		return JS_EXCEPTION;
	}

	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	REF_JS(proto);

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_keyboardEvent_class_id);
	JS_FreeValue(ctx, proto);

	if (JS_IsException(obj)) {
		goto fail;
	}
	keyb->keyCode = keyCode;
	JS_SetOpaque(obj, keyb);

	RETURN_JS(obj);

fail:
	JS_FreeValue(ctx, obj);
	mem_free(keyb);
	return JS_EXCEPTION;
}

static const JSCFunctionListEntry js_keyboardEvent_proto_funcs[] = {
	JS_CGETSET_DEF("key",	js_keyboardEvent_get_property_key, nullptr),
	JS_CGETSET_DEF("keyCode",	js_keyboardEvent_get_property_keyCode, nullptr)
};

static JSValue
js_keyboardEvent_get_property_key(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct keyboard *keyb = static_cast<struct keyboard *>(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

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

	struct keyboard *keyb = static_cast<struct keyboard *>(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

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
	keyCode = keyb->keyCode = get_kbd_key(ev);
	JSValue keyb_obj = JS_NewObjectClass(ctx, js_keyboardEvent_class_id);
	JS_SetPropertyFunctionList(ctx, keyb_obj, js_keyboardEvent_proto_funcs, countof(js_keyboardEvent_proto_funcs));
	JS_SetClassProto(ctx, js_keyboardEvent_class_id, keyb_obj);
	JS_SetOpaque(keyb_obj, keyb);

	JSValue rr = JS_DupValue(ctx, keyb_obj);
	RETURN_JS(rr);
}
