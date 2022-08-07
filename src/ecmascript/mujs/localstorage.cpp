/* The quickjs localstorage object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "config/home.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/localstorage-db.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/localstorage.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
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

#include <time.h>
#include "document/renderer.h"
#include "document/refresh.h"
#include "terminal/screen.h"

/* IMPLEMENTS READ FROM STORAGE USING SQLITE DATABASE */
static char *
readFromStorage(const char *key)
{

	char *val;

	if (local_storage_ready==0)
	{
		db_prepare_structure(local_storage_filename);
		local_storage_ready=1;
	}

	val = db_query_by_key(local_storage_filename, key);

	//DBG("Read: %s %s %s",local_storage_filename, key, val);

	return val;
}

static void
removeFromStorage(const char *key)
{
	if (local_storage_ready==0)
	{
		db_prepare_structure(local_storage_filename);
		local_storage_ready=1;
	}
	db_delete_from(local_storage_filename, key);
}

/* IMPLEMENTS SAVE TO STORAGE USING SQLITE DATABASE */
static void
saveToStorage(const char *key, const char *val)
{
	if (local_storage_ready==0) {
		db_prepare_structure(local_storage_filename);
		local_storage_ready=1;
	}

	int rows_affected=0;

	rows_affected=db_update_set(local_storage_filename, key, val);

	if (rows_affected==0) {
		rows_affected=db_insert_into(local_storage_filename, key, val);
	}

	// DBG(log, "UPD ROWS: %d KEY: %s VAL: %s",rows_affected,key,val);

}

static void
mjs_localstorage_getitem(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *key = js_tostring(J, 1);

	if (!key) {
		js_pushnull(J);
		return;
	}

	char *val = readFromStorage(key);

	if (!val) {
		js_pushnull(J);
		return;
	}
	// TODO possible memleak
	js_pushstring(J, val);
}

static void
mjs_localstorage_removeitem(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *key = js_tostring(J, 1);

	if (key) {
		removeFromStorage(key);
	}
	js_pushundefined(J);
}

static void
mjs_localstorage_setitem(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	const char *key_str = js_tostring(J, 1);
	const char *val_str = js_tostring(J, 2);

	if (!key_str || !val_str)
	{
		js_pushundefined(J);
		return;
	}
	saveToStorage(key_str, val_str);
#ifdef CONFIG_LEDS
	set_led_value(interpreter->vs->doc_view->session->status.ecmascript_led, 'J');
#endif
	js_pushundefined(J);
}

static void
mjs_localstorage_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[localstorage object]");
}

int
mjs_localstorage_init(js_State *J)
{
	js_newobject(J);
	{
		js_newcfunction(J, mjs_localstorage_getitem, "localStorage.getItem", 1);
		js_defproperty(J, -2, "getItem", JS_DONTENUM);

		js_newcfunction(J, mjs_localstorage_removeitem, "localStorage.removeItem", 1);
		js_defproperty(J, -2, "removeItem", JS_DONTENUM);

		js_newcfunction(J, mjs_localstorage_setitem, "localStorage.setItem", 2);
		js_defproperty(J, -2, "setItem", JS_DONTENUM);

		js_newcfunction(J, mjs_localstorage_toString, "localStorage.toString", 0);
		js_defproperty(J, -2, "toString", JS_DONTENUM);
	}
	js_defglobal(J, "localStorage", JS_DONTENUM);

	return 0;
}
