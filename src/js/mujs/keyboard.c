/* The MuJS KeyboardEvent object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/libdom/dom.h"

#include "document/libdom/doc.h"
#include "js/ecmascript.h"
#include "js/mujs.h"
#include "js/mujs/element.h"
#include "js/mujs/keyboard.h"
#include "intl/charsets.h"
#include "terminal/event.h"

static void mjs_keyboardEvent_get_property_code(js_State *J);
static void mjs_keyboardEvent_get_property_key(js_State *J);
static void mjs_keyboardEvent_get_property_keyCode(js_State *J);

static void mjs_keyboardEvent_get_property_bubbles(js_State *J);
static void mjs_keyboardEvent_get_property_cancelable(js_State *J);
//static void mjs_keyboardEvent_get_property_composed(js_State *J);
static void mjs_keyboardEvent_get_property_defaultPrevented(js_State *J);
static void mjs_keyboardEvent_get_property_target(js_State *J);
static void mjs_keyboardEvent_get_property_type(js_State *J);

static void mjs_keyboardEvent_preventDefault(js_State *J);

extern struct term_event last_event;

static void
mjs_keyboardEvent_finalizer(js_State *J, void *val)
{
	dom_keyboard_event *event = (dom_keyboard_event *)val;

	if (event) {
		dom_event_unref(event);
	}
}

void
mjs_push_keyboardEvent(js_State *J, struct term_event *ev, const char *type_)
{
	dom_keyboard_event *event = NULL;
	dom_exception exc = dom_keyboard_event_create(&event);

	if (exc != DOM_NO_ERR) {
		js_error(J, "error");
		return;
	}
	term_event_key_T keyCode = ev ? get_kbd_key(ev) : 0;
	dom_string *typ = NULL;
	const char *t = type_ ?: "keydown";

	exc = dom_string_create((const uint8_t *)t, strlen(t), &typ);
	dom_string *dom_key = NULL;
	convert_key_to_dom_string(keyCode, &dom_key);

	exc = dom_keyboard_event_init(event, typ, false, false,
		NULL, dom_key, NULL, DOM_KEY_LOCATION_STANDARD,
		false, false, false, false,
		false, false);

	if (typ) dom_string_unref(typ);
	if (dom_key) dom_string_unref(dom_key);

	js_newobject(J);
	{
		js_newuserdata(J, "event", event, mjs_keyboardEvent_finalizer);
		addmethod(J, "preventDefault", mjs_keyboardEvent_preventDefault, 0);
		addproperty(J, "bubbles", mjs_keyboardEvent_get_property_bubbles, NULL);
		addproperty(J, "cancelable", mjs_keyboardEvent_get_property_cancelable, NULL);
		addproperty(J, "code", mjs_keyboardEvent_get_property_code, NULL);
//		addproperty(J, "composed", mjs_keyboardEvent_get_property_composed, NULL);
		addproperty(J, "defaultPrevented", mjs_keyboardEvent_get_property_defaultPrevented, NULL);
		addproperty(J, "key", mjs_keyboardEvent_get_property_key, NULL);
		addproperty(J, "keyCode", mjs_keyboardEvent_get_property_keyCode, NULL);
		addproperty(J, "target", mjs_keyboardEvent_get_property_target, NULL);
		addproperty(J, "type", mjs_keyboardEvent_get_property_type, NULL);
	}
}

static void
mjs_keyboardEvent_get_property_bubbles(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_keyboard_event *event = (dom_keyboard_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	bool bubbles = false;
	(void)dom_event_get_bubbles(event, &bubbles);
	js_pushboolean(J, bubbles);
}

static void
mjs_keyboardEvent_get_property_cancelable(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_keyboard_event *event = (dom_keyboard_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	bool cancelable = false;
	(void)dom_event_get_cancelable(event, &cancelable);
	js_pushboolean(J, cancelable);
}

#if 0
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
#endif

static void
mjs_keyboardEvent_get_property_defaultPrevented(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_keyboard_event *event = (dom_keyboard_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	bool prevented = false;
	(void)dom_event_is_default_prevented(event, &prevented);
	js_pushboolean(J, prevented);
}

static void
mjs_keyboardEvent_get_property_key(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_keyboard_event *event = (dom_keyboard_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	dom_string *key = NULL;
	dom_exception exc = dom_keyboard_event_get_key(event, &key);

	if (exc != DOM_NO_ERR || !key) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, dom_string_data(key));
	dom_string_unref(key);
}

static void
mjs_keyboardEvent_get_property_keyCode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_keyboard_event *event = (dom_keyboard_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	dom_string *key = NULL;
	dom_exception exc = dom_keyboard_event_get_key(event, &key);

	if (exc != DOM_NO_ERR) {
		js_pushnull(J);
		return;
	}
	unicode_val_T keyCode = convert_dom_string_to_keycode(key);
	if (key) dom_string_unref(key);
	js_pushnumber(J, keyCode);
}

static void
mjs_keyboardEvent_get_property_code(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_keyboard_event *event = (dom_keyboard_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	dom_string *code = NULL;
	dom_exception exc = dom_keyboard_event_get_code(event, &code);

	if (exc != DOM_NO_ERR || !code) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, dom_string_data(code));
	dom_string_unref(code);
}

static void
mjs_keyboardEvent_get_property_target(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_keyboard_event *event = (dom_keyboard_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	dom_event_target *target = NULL;
	dom_exception exc = dom_event_get_target(event, &target);

	if (exc != DOM_NO_ERR || !target) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, target);
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(target);
}

static void
mjs_keyboardEvent_get_property_type(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_keyboard_event *event = (dom_keyboard_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	dom_string *typ = NULL;
	dom_exception exc = dom_event_get_type(event, &typ);

	if (exc != DOM_NO_ERR || !typ) {
		js_pushstring(J, "");
		return;
	}
	js_pushstring(J, dom_string_data(typ));
	dom_string_unref(typ);
}

static void
mjs_keyboardEvent_preventDefault(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_keyboard_event *event = (dom_keyboard_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	dom_event_prevent_default(event);
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
	dom_keyboard_event *event = NULL;
	dom_exception exc = dom_keyboard_event_create(&event);

	if (exc != DOM_NO_ERR) {
		js_error(J, "error");
		return;
	}
	dom_string *typ = NULL;
	const char *t = js_tostring(J, 1);

	if (t) {
		exc = dom_string_create((const uint8_t *)t, strlen(t), &typ);
	}
	bool bubbles = false;
	bool cancelable = false;
	dom_string *key = NULL;
	dom_string *code = NULL;

	js_getproperty(J, 2, "bubbles");
	bubbles = js_toboolean(J, -1);
	js_pop(J, 1);

	js_getproperty(J, 2, "cancelable");
	cancelable = js_toboolean(J, -1);
	js_pop(J, 1);

	js_getproperty(J, 2, "code");
	const char *c = js_tostring(J, -1);
	js_pop(J, 1);

	if (c) {
		exc = dom_string_create((const uint8_t *)c, strlen(c), &code);
	}

	js_getproperty(J, 2, "key");
	const char *k = js_tostring(J, -1);
	js_pop(J, 1);

	if (k) {
		exc = dom_string_create((const uint8_t *)k, strlen(k), &key);
	}
	exc = dom_keyboard_event_init(event, typ, bubbles, cancelable, NULL/*view*/,
		key, code, DOM_KEY_LOCATION_STANDARD,
		false, false, false,
		false, false, false);
	if (typ) dom_string_unref(typ);
	if (key) dom_string_unref(key);
	if (code) dom_string_unref(code);

	js_newobject(J);
	{
		js_newuserdata(J, "event", event, mjs_keyboardEvent_finalizer);
		addmethod(J, "preventDefault", mjs_keyboardEvent_preventDefault, 0);
		addproperty(J, "bubbles", mjs_keyboardEvent_get_property_bubbles, NULL);
		addproperty(J, "cancelable", mjs_keyboardEvent_get_property_cancelable, NULL);
		addproperty(J, "code", mjs_keyboardEvent_get_property_code, NULL);
//		addproperty(J, "composed", mjs_keyboardEvent_get_property_composed, NULL);
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
