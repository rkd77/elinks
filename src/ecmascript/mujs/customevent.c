/* The MuJS CustomEvent object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/ecmascript.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/customevent.h"
#include "intl/charsets.h"
#include "terminal/event.h"

static void mjs_customEvent_get_property_bubbles(js_State *J);
static void mjs_customEvent_get_property_cancelable(js_State *J);
static void mjs_customEvent_get_property_composed(js_State *J);
static void mjs_customEvent_get_property_defaultPrevented(js_State *J);
static void mjs_customEvent_get_property_detail(js_State *J);
static void mjs_customEvent_get_property_type(js_State *J);

static void mjs_customEvent_preventDefault(js_State *J);

struct eljscustom_event {
	const char *detail;
	char *type_;
	unsigned int bubbles:1;
	unsigned int cancelable:1;
	unsigned int composed:1;
	unsigned int defaultPrevented:1;
};

static void
mjs_customEvent_finalizer(js_State *J, void *val)
{
	struct eljscustom_event *event = (struct eljscustom_event *)val;

	if (event) {
		if (event->detail) {
			js_unref(J, event->detail);
		}
		mem_free_if(event->type_);
		mem_free(event);
	}
}

void
mjs_push_customEvent(js_State *J, char *type_)
{
	struct eljscustom_event *event = (struct eljscustom_event *)mem_calloc(1, sizeof(*event));

	if (!event) {
		js_error(J, "out of memory");
		return;
	}
	event->type_ = null_or_stracpy(type_);

	js_newobject(J);
	{
		js_newuserdata(J, "event", event, mjs_customEvent_finalizer);
		addmethod(J, "preventDefault", mjs_customEvent_preventDefault, 0);
		addproperty(J, "bubbles", mjs_customEvent_get_property_bubbles, NULL);
		addproperty(J, "cancelable", mjs_customEvent_get_property_cancelable, NULL);
		addproperty(J, "composed", mjs_customEvent_get_property_composed, NULL);
		addproperty(J, "defaultPrevented", mjs_customEvent_get_property_defaultPrevented, NULL);
		addproperty(J, "detail", mjs_customEvent_get_property_detail, NULL);
		addproperty(J, "type", mjs_customEvent_get_property_type, NULL);
	}
}

static void
mjs_customEvent_get_property_bubbles(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljscustom_event *event = (struct eljscustom_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	js_pushboolean(J, event->bubbles);
}

static void
mjs_customEvent_get_property_cancelable(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljscustom_event *event = (struct eljscustom_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	js_pushboolean(J, event->cancelable);
}

static void
mjs_customEvent_get_property_composed(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljscustom_event *event = (struct eljscustom_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	js_pushboolean(J, event->composed);
}

static void
mjs_customEvent_get_property_defaultPrevented(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljscustom_event *event = (struct eljscustom_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	js_pushboolean(J, event->defaultPrevented);
}

static void
mjs_customEvent_get_property_detail(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljscustom_event *event = (struct eljscustom_event *)js_touserdata(J, 0, "event");

	if (!event || !event->detail) {
		js_pushnull(J);
		return;
	}
	js_getregistry(J, event->detail);
}

static void
mjs_customEvent_get_property_type(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljscustom_event *event = (struct eljscustom_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, event->type_ ?: "");
}

static void
mjs_customEvent_preventDefault(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljscustom_event *event = (struct eljscustom_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	if (event->cancelable) {
		event->defaultPrevented = 1;
	}
	js_pushundefined(J);
}

static void
mjs_customEvent_fun(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushundefined(J);
}

static void
mjs_customEvent_constructor(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljscustom_event *event = (struct eljscustom_event *)mem_calloc(1, sizeof(*event));

	if (!event) {
		return;
	}
	event->type_ = null_or_stracpy(js_tostring(J, 1));

	js_getproperty(J, 2, "bubbles");
	event->bubbles = js_toboolean(J, -1);
	js_pop(J, 1);
	js_getproperty(J, 2, "cancelable");
	event->cancelable = js_toboolean(J, -1);
	js_pop(J, 1);
	js_getproperty(J, 2, "composed");
	event->composed = js_toboolean(J, -1);
	js_pop(J, 1);
	if (js_hasproperty(J, 2, "detail")) {
		event->detail = js_ref(J);
	}

	js_newobject(J);
	{
		js_newuserdata(J, "event", event, mjs_customEvent_finalizer);
		addmethod(J, "preventDefault", mjs_customEvent_preventDefault, 0);
		addproperty(J, "bubbles", mjs_customEvent_get_property_bubbles, NULL);
		addproperty(J, "cancelable", mjs_customEvent_get_property_cancelable, NULL);
		addproperty(J, "composed", mjs_customEvent_get_property_composed, NULL);
		addproperty(J, "defaultPrevented", mjs_customEvent_get_property_defaultPrevented, NULL);
		addproperty(J, "detail", mjs_customEvent_get_property_detail, NULL);
		addproperty(J, "type", mjs_customEvent_get_property_type, NULL);
	}
}

int
mjs_customEvent_init(js_State *J)
{
	js_pushglobal(J);
	js_newcconstructor(J, mjs_customEvent_fun, mjs_customEvent_constructor, "CustomEvent", 0);
	js_defglobal(J, "CustomEvent", JS_DONTENUM);
	return 0;
}
