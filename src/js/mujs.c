/* The mujs ECMAScript backend. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CONFIG_LIBDOM
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>
#endif

#include "elinks.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "config/options.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/libdom/renderer.h"
#include "document/libdom/renderer2.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/renderer.h"
#include "document/view.h"
#include "js/ecmascript.h"
#include "js/mujs.h"
#include "js/mujs/console.h"
#include "js/mujs/customevent.h"
#include "js/mujs/document.h"
#include "js/mujs/domparser.h"
#include "js/mujs/element.h"
#include "js/mujs/event.h"
#include "js/mujs/history.h"
#include "js/mujs/keyboard.h"
#include "js/mujs/localstorage.h"
#include "js/mujs/location.h"
#include "js/mujs/mapa.h"
#include "js/mujs/message.h"
#include "js/mujs/navigator.h"
#include "js/mujs/node.h"
#include "js/mujs/screen.h"
#include "js/mujs/unibar.h"
#include "js/mujs/url.h"
#include "js/mujs/window.h"
#include "js/mujs/xhr.h"
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
#include "util/memcount.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"

/*** Global methods */


static char mujs_version[32];

static void
mujs_init(struct module *module)
{
	snprintf(mujs_version, 31, "MuJS %d.%d.%d", JS_VERSION_MAJOR, JS_VERSION_MINOR, JS_VERSION_PATCH);
	module->name = mujs_version;

	map_attrs = attr_create_new_map();
	map_attributes = attr_create_new_map();
	map_rev_attributes = attr_create_new_map();
	map_collections = attr_create_new_map();
	map_rev_collections = attr_create_new_map();
	map_doctypes = attr_create_new_map();
	map_elements = attr_create_new_map();
	map_privates = attr_create_new_map();
	map_form_elements = attr_create_new_map();
	map_elements_form = attr_create_new_map();
	map_form = attr_create_new_map();
	map_rev_form = attr_create_new_map();
	map_forms = attr_create_new_map();
	map_rev_forms = attr_create_new_map();
	map_inputs = attr_create_new_map();
	//map_nodelist = attr_create_new_map();
	//map_rev_nodelist = attr_create_new_map();
	//js_module_init_ok = spidermonkey_runtime_addref();
}

static void
mujs_done(struct module *xxx)
{
	attr_delete_map(map_attrs);
	attr_delete_map(map_attributes);
	attr_delete_map(map_rev_attributes);
	attr_delete_map(map_collections);
	attr_delete_map(map_rev_collections);
	attr_delete_map(map_doctypes);
	attr_delete_map(map_elements);
	attr_delete_map(map_privates);
	attr_delete_map(map_form_elements);
	attr_delete_map(map_elements_form);
	attr_delete_map(map_form);
	attr_delete_map(map_rev_form);
	attr_delete_map(map_forms);
	attr_delete_map(map_rev_forms);
	attr_delete_map(map_inputs);
	//attr_delete_map(map_nodelist);
	//attr_delete_map(map_rev_nodelist);

//	if (js_module_init_ok)
//		spidermonkey_runtime_release();
}

void *
mujs_get_interpreter(struct ecmascript_interpreter *interpreter)
{
	assert(interpreter);

#ifdef CONFIG_DEBUG
	js_State *J = js_newstate(el_mujs_alloc, NULL, 0);
#else
	js_State *J = js_newstate(NULL, NULL, 0);
#endif
	if (!J) {
		return NULL;
	}
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	interpreter->backend_data = J;
	js_setcontext(J, interpreter);
	mjs_window_init(J);
	mjs_screen_init(J);
	mjs_unibar_init(J);
	mjs_navigator_init(J);
	mjs_history_init(J);
	mjs_console_init(J);
	mjs_localstorage_init(J);
	mjs_element_init(J);
	mjs_location_init(J);
	mjs_xhr_init(J);
	mjs_event_init(J);
	mjs_keyboardEvent_init(J);
	mjs_messageEvent_init(J);
	mjs_customEvent_init(J);
	mjs_url_init(J);
	mjs_domparser_init(J);
	mjs_node_init(J);

	mjs_push_document(J, document->dom);

	return J;
#if 0

	JSContext *ctx;
//	JSObject *console_obj, *document_obj, /* *forms_obj,*/ *history_obj, *location_obj,
//	         *statusbar_obj, *menubar_obj, *navigator_obj, *localstorage_obj, *screen_obj;

	assert(interpreter);
//	if (!js_module_init_ok) return NULL;

	JSRuntime *rt = JS_NewRuntime();
	if (!rt) {
		return nullptr;
	}

	JS_SetMemoryLimit(rt, 64 * 1024 * 1024);
	JS_SetGCThreshold(rt, 16 * 1024 * 1024);

	ctx = JS_NewContext(rt);

	if (!ctx) {
		JS_FreeRuntime(rt);
		return nullptr;
	}

	interpreter->backend_data = ctx;
	JS_SetContextOpaque(ctx, interpreter);

//	JS::SetWarningReporter(ctx, error_reporter);

	JS_SetInterruptHandler(rt, js_heartbeat_callback, interpreter);
//	JS::RealmOptions options;

//	JS::RootedObject window_obj(ctx, JS_NewGlobalObject(ctx, &window_class, NULL, JS::FireOnNewGlobalHook, options));

	js_window_init(ctx);
	js_screen_init(ctx);
	js_unibar_init(ctx);
	js_navigator_init(ctx);
	js_history_init(ctx);
	js_console_init(ctx);
	js_localstorage_init(ctx);
	js_element_init(ctx);

	interpreter->document_obj = js_document_init(ctx);
	interpreter->location_obj = js_location_init(ctx);

	return ctx;
#endif
}

void
mujs_put_interpreter(struct ecmascript_interpreter *interpreter)
{
	assert(interpreter);
	js_State *J = (js_State *)interpreter->backend_data;
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before js_gc: %s:%d\n", __FUNCTION__, __LINE__);
	js_gc(J, 0);
fprintf(stderr, "Before js_freestate: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	js_freestate(J);
	interpreter->backend_data = NULL;
}

#if 0
static void
error_reporter(struct ecmascript_interpreter *interpreter, JSContext *ctx)
{
	struct session *ses = interpreter->vs->doc_view->session;
	struct terminal *term;
	struct string msg;
	struct string f;

	assert(interpreter && interpreter->vs && interpreter->vs->doc_view
	       && ses && ses->tab);
	if_assert_failed return;

	term = ses->tab->term;

#ifdef CONFIG_LEDS
	set_led_value(ses->status.ecmascript_led, 'J');
#endif

	if (!get_opt_bool("ecmascript.error_reporting", ses)) {
		return;
	}

	if (init_string(&f)) {
		js_dump_error(ctx, &f);

		if (!init_string(&msg)) {
			done_string(&f);
			return;
		} else {
			add_to_string(&msg,
			_("A script embedded in the current document raised the following:\n", term));
			add_string_to_string(&msg, &f);
			done_string(&f);
			info_box(term, MSGBOX_FREE_TEXT, N_("JavaScript Error"), ALIGN_CENTER, msg.source);
		}
	}
}
#endif

void
mujs_eval(struct ecmascript_interpreter *interpreter,
                  struct string *code, struct string *ret)
{
	assert(interpreter);

	js_State *J = (js_State *)interpreter->backend_data;
	interpreter->ret = ret;
	js_dostring(J, code->source);
#if 0
	JSContext *ctx;

	assert(interpreter);
//	if (!js_module_init_ok) {
//		return;
//	}
	ctx = (JSContext *)interpreter->backend_data;
	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = ret;
	JSValue r = JS_Eval(ctx, code->source, code->length, "", 0);
	done_heartbeat(interpreter->heartbeat);

	if (JS_IsException(r)) {
		error_reporter(interpreter, ctx);
	}
#endif
}

void
mujs_call_function(struct ecmascript_interpreter *interpreter,
                  const char *fun, struct string *ret)
{
	js_State *J = (js_State *)interpreter->backend_data;
	interpreter->ret = ret;
	js_getregistry(J, fun); /* retrieve the js function from the registry */
	js_pushnull(J);
	js_pcall(J, 0);
	js_pop(J, 1);
}

char *
mujs_eval_stringback(struct ecmascript_interpreter *interpreter,
			     struct string *code)
{
	char *ret = NULL;
	assert(interpreter);

	js_State *J = (js_State *)interpreter->backend_data;
	interpreter->ret = NULL;

	js_loadstring(J, "[script]", code->source);
	js_pushundefined(J);
	js_pcall(J, 0);

	if (js_isundefined(J, -1)) {
		ret = NULL;
	} else if (js_isnull(J, -1)) {
		ret = NULL;
	} else {
		ret = stracpy(js_tostring(J, -1));
	}
	js_pop(J, 1);

	return ret;
#if 0
	JSContext *ctx;

	assert(interpreter);
//	if (!js_module_init_ok) {
//		return;
//	}
	ctx = (JSContext *)interpreter->backend_data;
	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = nullptr;
	JSValue r = JS_Eval(ctx, code->source, code->length, "", 0);
	done_heartbeat(interpreter->heartbeat);

	if (JS_IsNull(r)) {
		return nullptr;
	}

	if (JS_IsException(r)) {
		error_reporter(interpreter, ctx);
	}

	const char *str;
	char *string;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, r);

	if (!str) {
		return nullptr;
	}
	string = stracpy(str);
	JS_FreeCString(ctx, str);

	return string;
#endif
}

int
mujs_eval_boolback(struct ecmascript_interpreter *interpreter,
			   struct string *code)
{
	int ret = 0;
	assert(interpreter);

	js_State *J = (js_State *)interpreter->backend_data;
	interpreter->ret = NULL;

	js_loadstring(J, "[script]", code->source);
	js_pushundefined(J);
	js_pcall(J, 0);

	if (js_isundefined(J, -1)) {
		ret = -1;
	} else if (js_isnull(J, -1)) {
		ret = -1;
	} else {
		ret = js_toint32(J, -1);
	}
	js_pop(J, 1);

	return ret;
#if 0

	JSContext *ctx;

	assert(interpreter);
//	if (!js_module_init_ok) {
//		return;
//	}
	ctx = (JSContext *)interpreter->backend_data;
	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = nullptr;
	JSValue r = JS_Eval(ctx, code->source, code->length, "", 0);
	done_heartbeat(interpreter->heartbeat);

	if (JS_IsNull(r)) {
		return -1;
	}

	if (JS_IsUndefined(r)) {
		return -1;
	}

	int ret = -1;

	JS_ToInt32(ctx, &ret, r);

	return ret;
#endif
}

void
addmethod(js_State *J, const char *name, js_CFunction fun, int n)
{
	const char *realname = strchr(name, '.');
	realname = realname ? realname + 1 : name;
	js_newcfunction(J, fun, name, n);
	js_defproperty(J, -2, realname, JS_READONLY | JS_DONTENUM | JS_DONTCONF);
}

void addproperty(js_State *J, const char *name, js_CFunction getfun, js_CFunction setfun)
{
	const char *realname = strchr(name, '.');
	realname = realname ? realname + 1 : name;
	js_newcfunction(J, getfun, name, 0);
	if (setfun) {
		js_newcfunction(J, setfun, name, 1);
	} else {
		js_pushnull(J);
	}
	js_defaccessor(J, -3, realname, JS_READONLY | JS_DONTENUM | JS_DONTCONF);
}

struct module mujs_module = struct_module(
	/* name: */		N_("mujs"),
	/* options: */		NULL,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		mujs_init,
	/* done: */		mujs_done
);
