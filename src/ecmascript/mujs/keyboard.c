/* The MuJS KeyboardEvent object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/ecmascript.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/keyboard.h"
#include "intl/charsets.h"
#include "terminal/event.h"

static void mjs_keyboardEvent_get_property_key(js_State *J);
static void mjs_keyboardEvent_get_property_keyCode(js_State *J);

static void mjs_keyboardEvent_get_property_bubbles(js_State *J);
static void mjs_keyboardEvent_get_property_cancelable(js_State *J);
static void mjs_keyboardEvent_get_property_composed(js_State *J);
static void mjs_keyboardEvent_get_property_defaultPrevented(js_State *J);
static void mjs_keyboardEvent_get_property_type(js_State *J);

static void mjs_keyboardEvent_preventDefault(js_State *J);

static unicode_val_T keyCode;

struct keyboard {
	unicode_val_T keyCode;
	char *type_;
	unsigned int bubbles:1;
	unsigned int cancelable:1;
	unsigned int composed:1;
	unsigned int defaultPrevented:1;
};

extern struct term_event last_event;

static void
mjs_keyboardEvent_finalizer(js_State *J, void *val)
{
	struct keyboard *keyb = (struct keyboard *)val;

	if (keyb) {
		mem_free_if(keyb->type_);
		mem_free(keyb);
	}
}

void
mjs_push_keyboardEvent(js_State *J, struct term_event *ev, const char *type_)
{
	struct keyboard *keyb = (struct keyboard *)mem_calloc(1, sizeof(*keyb));

	if (!keyb) {
		js_error(J, "out of memory");
		return;
	}
	keyCode = keyb->keyCode = ev ? get_kbd_key(ev) : 0;
	keyb->type_ = null_or_stracpy(type_);

	js_newobject(J);
	{
		js_newuserdata(J, "event", keyb, mjs_keyboardEvent_finalizer);
		addmethod(J, "preventDefault", mjs_keyboardEvent_preventDefault, 0);
		addproperty(J, "bubbles", mjs_keyboardEvent_get_property_bubbles, NULL);
		addproperty(J, "cancelable", mjs_keyboardEvent_get_property_cancelable, NULL);
		addproperty(J, "composed", mjs_keyboardEvent_get_property_composed, NULL);
		addproperty(J, "defaultPrevented", mjs_keyboardEvent_get_property_defaultPrevented, NULL);
		addproperty(J, "key", mjs_keyboardEvent_get_property_key, NULL);
		addproperty(J, "keyCode", mjs_keyboardEvent_get_property_keyCode, NULL);
		addproperty(J, "type", mjs_keyboardEvent_get_property_type, NULL);
	}
}

static void
mjs_keyboardEvent_get_property_bubbles(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct keyboard *keyb = (struct keyboard *)js_touserdata(J, 0, "event");

	if (!keyb) {
		js_pushnull(J);
		return;
	}
	js_pushboolean(J, keyb->bubbles);
}

static void
mjs_keyboardEvent_get_property_cancelable(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct keyboard *keyb = (struct keyboard *)js_touserdata(J, 0, "event");

	if (!keyb) {
		js_pushnull(J);
		return;
	}
	js_pushboolean(J, keyb->cancelable);
}

static void
mjs_keyboardEvent_get_property_composed(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct keyboard *keyb = (struct keyboard *)js_touserdata(J, 0, "event");

	if (!keyb) {
		js_pushnull(J);
		return;
	}
	js_pushboolean(J, keyb->composed);
}

static void
mjs_keyboardEvent_get_property_defaultPrevented(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct keyboard *keyb = (struct keyboard *)js_touserdata(J, 0, "event");

	if (!keyb) {
		js_pushnull(J);
		return;
	}
	js_pushboolean(J, keyb->defaultPrevented);
}

static void
mjs_keyboardEvent_get_property_key(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct keyboard *keyb = (struct keyboard *)js_touserdata(J, 0, "event");

	if (!keyb) {
		js_pushnull(J);
		return;
	}
	char text[8] = {0};

	*text = keyb->keyCode;
	js_pushstring(J, text);
}

static void
mjs_keyboardEvent_get_property_keyCode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct keyboard *keyb = (struct keyboard *)js_touserdata(J, 0, "event");
	unicode_val_T code;

	if (!keyb) {
		js_pushnull(J);
		return;
	}
	code = keyb->keyCode ?: get_kbd_key(&last_event);

	if (code == KBD_ENTER) {
		code = 13;
	}
	js_pushnumber(J, code);
}

static void
mjs_keyboardEvent_get_property_type(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct keyboard *keyb = (struct keyboard *)js_touserdata(J, 0, "event");

	if (!keyb) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, keyb->type_ ?: "");
}

static void
mjs_keyboardEvent_preventDefault(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct keyboard *keyb = (struct keyboard *)js_touserdata(J, 0, "event");

	if (!keyb) {
		js_pushnull(J);
		return;
	}
	if (keyb->cancelable) {
		keyb->defaultPrevented = 1;
	}
	js_pushundefined(J);
}

static void
mjs_keyboardEvent_fun(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushundefined(J);
}

static void
mjs_keyboardEvent_constructor(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct keyboard *keyb = (struct keyboard *)mem_calloc(1, sizeof(*keyb));

	if (!keyb) {
		return;
	}
	keyb->type_ = null_or_stracpy(js_tostring(J, 1));

	js_getproperty(J, 2, "bubbles");
	keyb->bubbles = js_toboolean(J, -1);
	js_pop(J, 1);
	js_getproperty(J, 2, "cancelable");
	keyb->cancelable = js_toboolean(J, -1);
	js_pop(J, 1);
	js_getproperty(J, 2, "composed");
	keyb->composed = js_toboolean(J, -1);
	js_pop(J, 1);
	js_getproperty(J, 2, "keyCode");
	keyb->keyCode = js_toint32(J, -1);
	js_pop(J, 1);

	js_newobject(J);
	{
		js_newuserdata(J, "event", keyb, mjs_keyboardEvent_finalizer);
		addmethod(J, "preventDefault", mjs_keyboardEvent_preventDefault, 0);
		addproperty(J, "bubbles", mjs_keyboardEvent_get_property_bubbles, NULL);
		addproperty(J, "cancelable", mjs_keyboardEvent_get_property_cancelable, NULL);
		addproperty(J, "composed", mjs_keyboardEvent_get_property_composed, NULL);
		addproperty(J, "defaultPrevented", mjs_keyboardEvent_get_property_defaultPrevented, NULL);
		addproperty(J, "key", mjs_keyboardEvent_get_property_key, NULL);
		addproperty(J, "keyCode", mjs_keyboardEvent_get_property_keyCode, NULL);
		addproperty(J, "type", mjs_keyboardEvent_get_property_type, NULL);
	}
}

int
mjs_keyboardEvent_init(js_State *J)
{
	js_pushglobal(J);
	js_newcconstructor(J, mjs_keyboardEvent_fun, mjs_keyboardEvent_constructor, "KeyboardEvent", 0);
	js_defglobal(J, "KeyboardEvent", JS_DONTENUM);
	return 0;
}
