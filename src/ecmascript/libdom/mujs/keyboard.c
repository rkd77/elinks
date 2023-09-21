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

static unicode_val_T keyCode;

struct keyboard {
	unicode_val_T keyCode;
};

extern struct term_event last_event;

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
	struct keyboard *keyb = (struct keyboard *)mem_calloc(1, sizeof(*keyb));

	if (!keyb) {
		js_error(J, "out of memory");
		return;
	}
	keyCode = keyb->keyCode = ev ? get_kbd_key(ev) : 0;

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
	unicode_val_T code;

	if (!keyb) {
		js_pushnull(J);
		return;
	}
	code = keyb->keyCode ?: get_kbd_key(&last_event);
	js_pushnumber(J, code);
}
