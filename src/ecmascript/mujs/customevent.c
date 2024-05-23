/* The MuJS CustomEvent object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/document.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/libdom/dom.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/customevent.h"
#include "ecmascript/mujs/element.h"
#include "intl/charsets.h"
#include "terminal/event.h"
#include "viewer/text/vs.h"

static void mjs_customEvent_get_property_bubbles(js_State *J);
static void mjs_customEvent_get_property_cancelable(js_State *J);
//static void mjs_customEvent_get_property_composed(js_State *J);
static void mjs_customEvent_get_property_defaultPrevented(js_State *J);
static void mjs_customEvent_get_property_detail(js_State *J);
static void mjs_customEvent_get_property_target(js_State *J);
static void mjs_customEvent_get_property_type(js_State *J);

static void mjs_customEvent_preventDefault(js_State *J);

static void
mjs_customEvent_finalizer(js_State *J, void *val)
{
	dom_custom_event *event = (dom_custom_event *)val;

	if (event) {
		const char *detail = NULL;
		dom_exception exc = dom_custom_event_get_detail(event, &detail);

		if (detail) {
			js_unref(J, detail);
		}
		dom_event_unref(event);
	}
}

void
mjs_push_customEvent(js_State *J, char *type_)
{
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
		js_error(J, "error");
		return;
	}
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		js_error(J, "error");
		return;
	}
	dom_custom_event *event = NULL;
	dom_string *CustomEventStr = NULL;
	dom_exception exc = dom_string_create("CustomEvent", sizeof("CustomEvent") - 1, &CustomEventStr);

	if (exc != DOM_NO_ERR || !CustomEventStr) {
		js_error(J, "error");
		return;
	}
	exc = dom_document_event_create_event(document->dom, CustomEventStr, &event);
	dom_string_unref(CustomEventStr);

	if (exc != DOM_NO_ERR) {
		js_error(J, "error");
		return;
	}
	dom_string *typ = NULL;

	if (type_) {
		exc = dom_string_create((const uint8_t *)type_, strlen(type_), &typ);
	}
	if (exc != DOM_NO_ERR) {
		js_error(J, "out of memory");
		return;
	}
	dom_custom_event_init_ns(event, NULL, typ, false, false, NULL);

	if (typ) {
		dom_string_unref(typ);
	}

	js_newobject(J);
	{
		js_newuserdata(J, "event", event, mjs_customEvent_finalizer);
		addmethod(J, "preventDefault", mjs_customEvent_preventDefault, 0);
		addproperty(J, "bubbles", mjs_customEvent_get_property_bubbles, NULL);
		addproperty(J, "cancelable", mjs_customEvent_get_property_cancelable, NULL);
//		addproperty(J, "composed", mjs_customEvent_get_property_composed, NULL);
		addproperty(J, "defaultPrevented", mjs_customEvent_get_property_defaultPrevented, NULL);
		addproperty(J, "detail", mjs_customEvent_get_property_detail, NULL);
		addproperty(J, "target", mjs_customEvent_get_property_target, NULL);
		addproperty(J, "type", mjs_customEvent_get_property_type, NULL);
	}
}

static void
mjs_customEvent_get_property_bubbles(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_custom_event *event = (dom_custom_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	bool bubbles = false;
	dom_exception exc = dom_event_get_bubbles(event, &bubbles);
	js_pushboolean(J, bubbles);
}

static void
mjs_customEvent_get_property_cancelable(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_custom_event *event = (dom_custom_event *)js_touserdata(J, 0, "event");

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
#endif

static void
mjs_customEvent_get_property_defaultPrevented(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_custom_event *event = (dom_custom_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	bool prevented = false;
	dom_exception exc = dom_event_is_default_prevented(event, &prevented);
	js_pushboolean(J, prevented);
}

static void
mjs_customEvent_get_property_detail(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_custom_event *event = (dom_custom_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	const char *detail = NULL;
	dom_exception exc = dom_custom_event_get_detail(event, &detail);

	if (exc != DOM_NO_ERR || !detail) {
		js_pushnull(J);
		return;
	}
	js_getregistry(J, detail);
}

static void
mjs_customEvent_get_property_target(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_custom_event *event = (dom_custom_event *)js_touserdata(J, 0, "event");

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
	dom_node_unref(target);
}

static void
mjs_customEvent_get_property_type(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_custom_event *event = (dom_custom_event *)js_touserdata(J, 0, "event");

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
mjs_customEvent_preventDefault(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_custom_event *event = (dom_custom_event *)js_touserdata(J, 0, "event");

	if (!event) {
		js_pushnull(J);
		return;
	}
	dom_event_prevent_default(event);
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;
	if (!vs) {
		js_error(J, "error");
		return;
	}
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		js_error(J, "error");
		return;
	}
	dom_custom_event *event = NULL;
	dom_string *CustomEventStr = NULL;
	dom_exception exc = dom_string_create("CustomEvent", sizeof("CustomEvent") - 1, &CustomEventStr);

	if (exc != DOM_NO_ERR || !CustomEventStr) {
		js_error(J, "error");
		return;
	}
	exc = dom_document_event_create_event(document->dom, CustomEventStr, &event);
	dom_string_unref(CustomEventStr);

	if (exc != DOM_NO_ERR) {
		js_error(J, "error");
		return;
	}
	dom_string *typ = NULL;
	const char *tt = js_tostring(J, 1);

	if (!tt) {
		js_error(J, "error");
		return;
	}
	exc = dom_string_create((const uint8_t *)tt, strlen(tt), &typ);
	bool bubbles = false;
	bool cancelable = false;

	const char *detail = NULL;

	js_getproperty(J, 2, "bubbles");
	bubbles = js_toboolean(J, -1);
	js_pop(J, 1);
	js_getproperty(J, 2, "cancelable");
	cancelable = js_toboolean(J, -1);
	js_pop(J, 1);

	if (js_hasproperty(J, 2, "detail")) {
		detail = js_ref(J);
	}
	exc = dom_custom_event_init_ns(event, NULL, typ, bubbles, cancelable, detail);

	if (typ) {
		dom_string_unref(typ);
	}
	js_newobject(J);
	{
		js_newuserdata(J, "event", event, mjs_customEvent_finalizer);
		addmethod(J, "preventDefault", mjs_customEvent_preventDefault, 0);
		addproperty(J, "bubbles", mjs_customEvent_get_property_bubbles, NULL);
		addproperty(J, "cancelable", mjs_customEvent_get_property_cancelable, NULL);
//		addproperty(J, "composed", mjs_customEvent_get_property_composed, NULL);
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
