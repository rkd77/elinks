/* The MuJS MessageEvent object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/ecmascript.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/message.h"

static void mjs_messageEvent_get_property_data(js_State *J);
static void mjs_messageEvent_get_property_lastEventId(js_State *J);
static void mjs_messageEvent_get_property_origin(js_State *J);
static void mjs_messageEvent_get_property_source(js_State *J);

static void mjs_messageEvent_get_property_bubbles(js_State *J);
static void mjs_messageEvent_get_property_cancelable(js_State *J);
static void mjs_messageEvent_get_property_composed(js_State *J);
static void mjs_messageEvent_get_property_defaultPrevented(js_State *J);
static void mjs_messageEvent_get_property_type(js_State *J);

static void mjs_messageEvent_preventDefault(js_State *J);

struct message_event {
	char *data;
	char *lastEventId;
	char *origin;
	char *source;

	char *type_;
	unsigned int bubbles:1;
	unsigned int cancelable:1;
	unsigned int composed:1;
	unsigned int defaultPrevented:1;
};

static
void mjs_messageEvent_finalizer(js_State *J, void *val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct message_event *event = (struct message_event *)val;

	if (event) {
		mem_free_if(event->data);
		mem_free_if(event->lastEventId);
		mem_free_if(event->origin);
		mem_free_if(event->source);
		mem_free_if(event->type_);
		mem_free(event);
	}
}

static int lastEventId;

void
mjs_push_messageEvent(js_State *J, char *data, char *origin, char *source)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct message_event *event = (struct message_event *)mem_calloc(1, sizeof(*event));

	if (!event) {
		js_error(J, "out of memory");
		return;
	}
	event->data = null_or_stracpy(data);
	event->origin = null_or_stracpy(origin);
	event->source = null_or_stracpy(source);

	char id[32];

	snprintf(id, 31, "%d", ++lastEventId);
	event->lastEventId = stracpy(id);

	js_newobject(J);
	{
		js_newuserdata(J, "event", event, mjs_messageEvent_finalizer);
		addmethod(J, "preventDefault", mjs_messageEvent_preventDefault, 0);
		addproperty(J, "bubbles", mjs_messageEvent_get_property_bubbles, NULL);
		addproperty(J, "cancelable", mjs_messageEvent_get_property_cancelable, NULL);
		addproperty(J, "composed", mjs_messageEvent_get_property_composed, NULL);
		addproperty(J, "data", mjs_messageEvent_get_property_data, NULL);
		addproperty(J, "defaultPrevented", mjs_messageEvent_get_property_defaultPrevented, NULL);
		addproperty(J, "lastEventId", mjs_messageEvent_get_property_lastEventId, NULL);
		addproperty(J, "origin", mjs_messageEvent_get_property_origin, NULL);
		addproperty(J, "source", mjs_messageEvent_get_property_source, NULL);
		addproperty(J, "type", mjs_messageEvent_get_property_type, NULL);
	}
}

static void
mjs_messageEvent_get_property_bubbles(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct message_event *event = (struct message_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	js_pushboolean(J, event->bubbles);
}

static void
mjs_messageEvent_get_property_cancelable(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct message_event *event = (struct message_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	js_pushboolean(J, event->cancelable);
}

static void
mjs_messageEvent_get_property_composed(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct message_event *event = (struct message_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	js_pushboolean(J, event->composed);
}

static void
mjs_messageEvent_get_property_data(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct message_event *event = (struct message_event *)js_touserdata(J, 0, "event");

	if (!event || !event->data) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, event->data);
}

static void
mjs_messageEvent_get_property_defaultPrevented(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct message_event *event = (struct message_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	js_pushboolean(J, event->defaultPrevented);
}

static void
mjs_messageEvent_get_property_lastEventId(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct message_event *event = (struct message_event *)js_touserdata(J, 0, "event");

	if (!event || !event->lastEventId) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, event->lastEventId);
}

static void
mjs_messageEvent_get_property_origin(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct message_event *event = (struct message_event *)js_touserdata(J, 0, "event");

	if (!event || !event->origin) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, event->origin);
}

static void
mjs_messageEvent_get_property_source(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct message_event *event = (struct message_event *)js_touserdata(J, 0, "event");

	if (!event || !event->source) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, event->source);
}

static void
mjs_messageEvent_get_property_type(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct message_event *event = (struct message_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, event->type_ ?: "");
}

static void
mjs_messageEvent_preventDefault(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct message_event *event = (struct message_event *)js_touserdata(J, 0, "event");

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
mjs_messageEvent_fun(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushundefined(J);
}

static void
mjs_messageEvent_constructor(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct message_event *event = (struct message_event *)mem_calloc(1, sizeof(*event));

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

	if (js_hasproperty(J, 2, "data")) {
		event->data = null_or_stracpy(js_tostring(J, -1));
		js_pop(J, 1);
	}

	if (js_hasproperty(J, 2, "lastEventId")) {
		event->lastEventId = null_or_stracpy(js_tostring(J, -1));
		js_pop(J, 1);
	}

	if (js_hasproperty(J, 2, "origin")) {
		event->origin = null_or_stracpy(js_tostring(J, -1));
		js_pop(J, 1);
	}

	if (js_hasproperty(J, 2, "source")) {
		event->source = null_or_stracpy(js_tostring(J, -1));
		js_pop(J, 1);
	}

	js_newobject(J);
	{
		js_newuserdata(J, "event", event, mjs_messageEvent_finalizer);
		addmethod(J, "preventDefault", mjs_messageEvent_preventDefault, 0);
		addproperty(J, "bubbles", mjs_messageEvent_get_property_bubbles, NULL);
		addproperty(J, "cancelable", mjs_messageEvent_get_property_cancelable, NULL);
		addproperty(J, "composed", mjs_messageEvent_get_property_composed, NULL);
		addproperty(J, "data", mjs_messageEvent_get_property_data, NULL);
		addproperty(J, "defaultPrevented", mjs_messageEvent_get_property_defaultPrevented, NULL);
		addproperty(J, "lastEventId", mjs_messageEvent_get_property_lastEventId, NULL);
		addproperty(J, "origin", mjs_messageEvent_get_property_origin, NULL);
		addproperty(J, "source", mjs_messageEvent_get_property_source, NULL);
		addproperty(J, "type", mjs_messageEvent_get_property_type, NULL);
	}
}

int
mjs_messageEvent_init(js_State *J)
{
	js_pushglobal(J);
	js_newcconstructor(J, mjs_messageEvent_fun, mjs_messageEvent_constructor, "MessageEvent", 0);
	js_defglobal(J, "MessageEvent", JS_DONTENUM);
	return 0;
}
