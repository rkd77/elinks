/* The quickjs localstorage object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/leds.h"
#include "document/view.h"
#include "js/ecmascript.h"
#include "js/localstorage-db.h"
#include "js/mujs.h"
#include "js/mujs/localstorage.h"
#include "session/session.h"
#include "viewer/text/vs.h"

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
	js_pushstring(J, val);
	mem_free(val);
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
		addmethod(J, "localStorage.getItem", mjs_localstorage_getitem, 1);
		addmethod(J, "localStorage.removeItem", mjs_localstorage_removeitem, 1);
		addmethod(J, "localStorage.setItem", mjs_localstorage_setitem, 2);
		addmethod(J, "localStorage.toString", mjs_localstorage_toString, 0);
	}
	js_defglobal(J, "localStorage", JS_DONTENUM);

	return 0;
}
