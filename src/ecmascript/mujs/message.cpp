/* The MuJS MessageEvent object implementation. */

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
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/message.h"
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

static void mjs_messageEvent_get_property_data(js_State *J);
static void mjs_messageEvent_get_property_lastEventId(js_State *J);
static void mjs_messageEvent_get_property_origin(js_State *J);
static void mjs_messageEvent_get_property_source(js_State *J);

struct message_event {
	char *data;
	char *lastEventId;
	char *origin;
	char *source;
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

		addproperty(J, "data", mjs_messageEvent_get_property_data, NULL);
		addproperty(J, "lastEventId", mjs_messageEvent_get_property_lastEventId, NULL);
		addproperty(J, "origin", mjs_messageEvent_get_property_origin, NULL);
		addproperty(J, "source", mjs_messageEvent_get_property_source, NULL);
	}
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
