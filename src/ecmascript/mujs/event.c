/* The MuJS Event object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/ecmascript.h"
#include "ecmascript/libdom/dom.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/event.h"
#include "intl/charsets.h"
#include "terminal/event.h"

static void mjs_event_get_property_bubbles(js_State *J);
static void mjs_event_get_property_cancelable(js_State *J);
//static void mjs_event_get_property_composed(js_State *J);
static void mjs_event_get_property_defaultPrevented(js_State *J);
static void mjs_event_get_property_type(js_State *J);

static void mjs_event_preventDefault(js_State *J);

static void
mjs_event_finalizer(js_State *J, void *val)
{
	dom_event *event = (dom_event *)val;

	if (event) {
		dom_event_unref(event);
	}
}

void
mjs_push_event(js_State *J, char *type_)
{
	dom_event *event = NULL;
	dom_exception exc = dom_event_create(&event);

	if (exc != DOM_NO_ERR) {
		js_error(J, "out of memory");
		return;
	}

	if (type_) {
		dom_string *typ = NULL;
		exc = dom_string_create(type_, strlen(type_), &typ);
		dom_event_init(event, typ, false, false);
		if (typ) dom_string_unref(typ);
	}
	js_newobject(J);
	{
		js_newuserdata(J, "event", event, mjs_event_finalizer);
		addmethod(J, "preventDefault", mjs_event_preventDefault, 0);
		addproperty(J, "bubbles", mjs_event_get_property_bubbles, NULL);
		addproperty(J, "cancelable", mjs_event_get_property_cancelable, NULL);
//		addproperty(J, "composed", mjs_event_get_property_composed, NULL);
		addproperty(J, "defaultPrevented", mjs_event_get_property_defaultPrevented, NULL);
		addproperty(J, "type", mjs_event_get_property_type, NULL);
	}
}

static void
mjs_event_get_property_bubbles(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_event *event = (dom_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	bool bubbles = false;
	dom_exception exc = dom_event_get_bubbles(event, &bubbles);
	js_pushboolean(J, bubbles);
}

static void
mjs_event_get_property_cancelable(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_event *event = (dom_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	bool cancelable = false;
	dom_exception exc = dom_event_get_cancelable(event, &cancelable);
	js_pushboolean(J, cancelable);
}

#if 0
static void
mjs_event_get_property_composed(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_event *event = (struct eljs_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	js_pushboolean(J, event->composed);
}
#endif

static void
mjs_event_get_property_defaultPrevented(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_event *event = (dom_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	bool prevented = false;
	dom_exception exc = dom_event_is_default_prevented(event, &prevented);
	js_pushboolean(J, prevented);
}

static void
mjs_event_get_property_type(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_event *event = (dom_event *)js_touserdata(J, 0, "event");

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
mjs_event_preventDefault(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_event *event = (dom_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	dom_event_prevent_default(event);
	js_pushundefined(J);
}

static void
mjs_event_fun(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushundefined(J);
}

static void
mjs_event_constructor(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_event *event = NULL;
	dom_exception exc = dom_event_create(&event);

	if (exc != DOM_NO_ERR) {
		return;
	}
	dom_string *typ = NULL;
	const char *str = js_tostring(J, 1);

	if (str) {
		exc = dom_string_create(str, strlen(str), &typ);
	}
	js_getproperty(J, 2, "bubbles");
	bool bubbles = js_toboolean(J, -1);
	js_pop(J, 1);
	js_getproperty(J, 2, "cancelable");
	bool cancelable = js_toboolean(J, -1);
	js_pop(J, 1);
//	js_getproperty(J, 2, "composed");
//	event->composed = js_toboolean(J, -1);
//	js_pop(J, 1);
	exc = dom_event_init(event, typ, bubbles, cancelable);

	js_newobject(J);
	{
		js_newuserdata(J, "event", event, mjs_event_finalizer);
		addmethod(J, "preventDefault", mjs_event_preventDefault, 0);
		addproperty(J, "bubbles", mjs_event_get_property_bubbles, NULL);
		addproperty(J, "cancelable", mjs_event_get_property_cancelable, NULL);
//		addproperty(J, "composed", mjs_event_get_property_composed, NULL);
		addproperty(J, "defaultPrevented", mjs_event_get_property_defaultPrevented, NULL);
		addproperty(J, "type", mjs_event_get_property_type, NULL);
	}
}

int
mjs_event_init(js_State *J)
{
	js_pushglobal(J);
	js_newcconstructor(J, mjs_event_fun, mjs_event_constructor, "Event", 0);
	js_defglobal(J, "Event", JS_DONTENUM);
	return 0;
}
