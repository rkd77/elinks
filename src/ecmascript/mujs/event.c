/* The MuJS Event object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/ecmascript.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/event.h"
#include "intl/charsets.h"
#include "terminal/event.h"

static void mjs_event_get_property_bubbles(js_State *J);
static void mjs_event_get_property_cancelable(js_State *J);
static void mjs_event_get_property_composed(js_State *J);
static void mjs_event_get_property_defaultPrevented(js_State *J);
static void mjs_event_get_property_type(js_State *J);

static void mjs_event_preventDefault(js_State *J);

struct eljs_event {
	char *type_;
	unsigned int bubbles:1;
	unsigned int cancelable:1;
	unsigned int composed:1;
	unsigned int defaultPrevented:1;
};

static void
mjs_event_finalizer(js_State *J, void *val)
{
	struct eljs_event *event = (struct eljs_event *)val;

	if (event) {
		mem_free_if(event->type_);
		mem_free(event);
	}
}

void
mjs_push_event(js_State *J, char *type_)
{
	struct eljs_event *event = (struct eljs_event *)mem_calloc(1, sizeof(*event));

	if (!event) {
		js_error(J, "out of memory");
		return;
	}
	event->type_ = null_or_stracpy(type_);

	js_newobject(J);
	{
		js_newuserdata(J, "event", event, mjs_event_finalizer);
		addmethod(J, "preventDefault", mjs_event_preventDefault, 0);
		addproperty(J, "bubbles", mjs_event_get_property_bubbles, NULL);
		addproperty(J, "cancelable", mjs_event_get_property_cancelable, NULL);
		addproperty(J, "composed", mjs_event_get_property_composed, NULL);
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
	struct eljs_event *event = (struct eljs_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	js_pushboolean(J, event->bubbles);
}

static void
mjs_event_get_property_cancelable(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_event *event = (struct eljs_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	js_pushboolean(J, event->cancelable);
}

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

static void
mjs_event_get_property_defaultPrevented(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_event *event = (struct eljs_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	js_pushboolean(J, event->defaultPrevented);
}

static void
mjs_event_get_property_type(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_event *event = (struct eljs_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, event->type_ ?: "");
}

static void
mjs_event_preventDefault(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_event *event = (struct eljs_event *)js_touserdata(J, 0, "event");

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
	struct eljs_event *event = (struct eljs_event *)mem_calloc(1, sizeof(*event));

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

	js_newobject(J);
	{
		js_newuserdata(J, "event", event, mjs_event_finalizer);
		addmethod(J, "preventDefault", mjs_event_preventDefault, 0);
		addproperty(J, "bubbles", mjs_event_get_property_bubbles, NULL);
		addproperty(J, "cancelable", mjs_event_get_property_cancelable, NULL);
		addproperty(J, "composed", mjs_event_get_property_composed, NULL);
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
