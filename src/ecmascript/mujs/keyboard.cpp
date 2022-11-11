/* The MuJS KeyboardEvent object implementation. */

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
#include "ecmascript/mujs/keyboard.h"
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


static void mjs_keyboardEvent_get_property_key(js_State *J);
static void mjs_keyboardEvent_get_property_keyCode(js_State *J);

static unicode_val_T keyCode;

struct keyboard {
	unicode_val_T keyCode;
};

static void
mjs_keyboardEvent_finalizer(js_State *J, void *val)
{
	struct keyboard *keyb = (struct keyboard *)val;

	if (keyb) {
		mem_free(keyb);
	}
}

void
mjs_push_keyboardEvent(js_State *J, struct term_event *ev)
{
	struct keyboard *keyb = mem_calloc(1, sizeof(*keyb));

	if (!keyb) {
		js_pushnull(J);
		return;
	}
	keyCode = keyb->keyCode = get_kbd_key(ev);

	js_newobject(J);
	{
		js_newuserdata(J, "event", keyb, mjs_keyboardEvent_finalizer);
		addproperty(J, "key", mjs_keyboardEvent_get_property_key, NULL);
		addproperty(J, "keyCode", mjs_keyboardEvent_get_property_keyCode, NULL);
	}
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

	if (!keyb) {
		js_pushnull(J);
		return;
	}
	js_pushnumber(J, keyb->keyCode);
}
