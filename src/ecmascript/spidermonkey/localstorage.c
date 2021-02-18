/* The SpiderMonkey localstorage object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"

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
#include "ecmascript/spidermonkey/form.h"
#include "ecmascript/spidermonkey/location.h"
#include "ecmascript/spidermonkey/localstorage.h"
#include "ecmascript/spidermonkey/localstorage-db.h"
#include "ecmascript/spidermonkey/document.h"
#include "ecmascript/spidermonkey/window.h"
#include "intl/gettext/libintl.h"
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

#define DEBUG 1

/* IMPLEMENTS READ FROM STORAGE USING SQLITE DATABASE */
static unsigned char * readFromStorage(unsigned char *key) {
  char db_name[8192]="";
  char * val = "";
  val = (unsigned char *)  malloc (8192);
  strcat(db_name,elinks_home);
  strcat(db_name,"lc_elinks.db");
  //logger(db_name);
  char log[8192]="";
  val = qry_db(db_name, key);
  sprintf(log,"Read: %s %s %s",db_name, key, val);
  //logger(log);
  return (val);
}

/* IMPLEMENTS SAVE TO STORAGE USING FILE ACCESS */
static void saveToStorage(unsigned char *key, unsigned char *val) {
  char db_name[8192]="";
  strcat(db_name,elinks_home);
  strcat(db_name,"lc_elinks.db");
  int rows_affected=0;
  char log[8192]="";

  rows_affected=update_db(db_name, key, val);
  sprintf(log,"UPD ROWS: %d KEY: %s VAL: %s",rows_affected,key,val);
  //logger(log);
  if (rows_affected==0) {
    rows_affected=insert_db(db_name, key, val);
  }
}

static bool localstorage_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);

//static bool localstorage_set_property(JSContext *ctx, JSObject *obj, jsid id, JSBool strict, jsval *vp);

JSClassOps localstorage_ops = {
	JS_PropertyStub, nullptr,
	localstorage_get_property, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr
};

/* Each @localstorage_class object must have a @window_class parent.  */
const JSClass localstorage_class = {
	"localStorage",
	JSCLASS_HAS_PRIVATE,
	&localstorage_ops
	//JS_PropertyStub, JS_PropertyStub,
	//JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

/* Tinyids of properties.  Use negative values to distinguish these
 * from array indexes (even though this object has no array elements).
 * ECMAScript code should not use these directly as in document[-1];
 * future versions of ELinks may change the numbers.  */
//enum localstorage_prop {
//	JSP_LOCALSTORAGE_LOC   = -1,
//	JSP_LOCALSTORAGE_REF   = -2,
//	JSP_LOCALSTORAGE_TITLE = -3,
//	JSP_LOCALSTORAGE_URL   = -4,
//};
/* "cookie" is special; it isn't a regular property but we channel it to the
 * cookie-module. XXX: Would it work if "cookie" was defined in this array? */
const JSPropertySpec localstorage_props[] = {
	{ NULL }
};

///* @localstorage_class.getProperty */
static bool
localstorage_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;

	return(true);
}

static bool localstorage_setitem(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool localstorage_getitem(JSContext *ctx, unsigned int argc, JS::Value *vp);

const spidermonkeyFunctionSpec localstorage_funcs[] = {
	{ "setItem",		localstorage_setitem,	2 },
	{ "getItem",		localstorage_getitem,	1 },
	{ NULL }
};

static bool 
localstorage_getitem(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	//jsval val;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
	JS::CallArgs args = CallArgsFromVp(argc, vp);
        unsigned char *key = JS_EncodeString(ctx, args[0].toString());
	//DBG("localstorage get by key: %s\n", args);

	if (argc != 1) {
	       args.rval().setBoolean(false);
	       return(true);
	}

	unsigned char *val;
        val = (unsigned char * ) malloc(32000);
        val = readFromStorage(key);

	//DBG("%s %s\n", key, val);

        if (strlen(val)>0) {
		//static unsigned char val[32000];
		//strncpy(val, cookies->source, 1023);
		args.rval().setString(JS_NewStringCopyZ(ctx, val));
		//mem_free(val);
	} else {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
	
	}

	return(true);

}

/* @localstorage_funcs{"setItem"} */
static bool
localstorage_setitem(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
	JS::CallArgs args = CallArgsFromVp(argc, vp);

        if (argc != 2) {
	       args.rval().setBoolean(false);
	       return(true);
        }

        unsigned char *key = JS_EncodeString(ctx, args[0].toString());
        unsigned char *val = JS_EncodeString(ctx, args[1].toString());
	saveToStorage(key,val);
	//DBG("%s %s\n", key, val);


#ifdef CONFIG_LEDS
	set_led_value(interpreter->vs->doc_view->session->status.ecmascript_led, 'J');
#endif

	args.rval().setBoolean(true);

	return(true);
}
