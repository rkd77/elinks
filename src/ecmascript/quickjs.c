/* The Quickjs ECMAScript backend. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#include "ecmascript/ecmascript.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/console.h"
#include "ecmascript/quickjs/customevent.h"
#include "ecmascript/quickjs/document.h"
#include "ecmascript/quickjs/domparser.h"
#include "ecmascript/quickjs/element.h"
#include "ecmascript/quickjs/event.h"
#include "ecmascript/quickjs/heartbeat.h"
#include "ecmascript/quickjs/history.h"
#include "ecmascript/quickjs/keyboard.h"
#include "ecmascript/quickjs/localstorage.h"
#include "ecmascript/quickjs/location.h"
#include "ecmascript/quickjs/mapa.h"
#include "ecmascript/quickjs/message.h"
#include "ecmascript/quickjs/navigator.h"
#include "ecmascript/quickjs/screen.h"
#include "ecmascript/quickjs/unibar.h"
#include "ecmascript/quickjs/url.h"
#include "ecmascript/quickjs/urlsearchparams.h"
#include "ecmascript/quickjs/window.h"
#include "ecmascript/quickjs/xhr.h"
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

#include "ecmascript/fetch.h"

/*** Global methods */

static void
quickjs_init(struct module *xxx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	map_attrs = attr_create_new_attrs_map();
	map_attributes = attr_create_new_attributes_map();
	map_rev_attributes = attr_create_new_attributes_map_rev();
	map_collections = attr_create_new_collections_map();
	map_rev_collections = attr_create_new_collections_map_rev();
	map_doctypes = attr_create_new_doctypes_map();
	map_elements = attr_create_new_elements_map();
	map_privates = attr_create_new_privates_map_void();
	map_form = attr_create_new_form_map();
	map_form_rev = attr_create_new_form_map_rev();
	map_forms = attr_create_new_forms_map();
	map_rev_forms = attr_create_new_forms_map_rev();
	map_inputs = attr_create_new_input_map();
	map_nodelist = attr_create_new_nodelist_map();
	map_rev_nodelist = attr_create_new_nodelist_map_rev();

	map_form_elements = attr_create_new_form_elements_map();
	map_form_elements_rev = attr_create_new_form_elements_map_rev();
	//js_module_init_ok = spidermonkey_runtime_addref();

	map_csses = attr_create_new_csses_map();
	map_rev_csses = attr_create_new_csses_map_rev();
}

static void
quickjs_done(struct module *xxx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	attr_delete_map(map_attrs);
	attr_delete_map(map_attributes);
	attr_delete_map_rev(map_rev_attributes);
	attr_delete_map(map_collections);
	attr_delete_map_rev(map_rev_collections);
	attr_delete_map(map_doctypes);
	attr_delete_map(map_elements);
	map_elements = NULL;
	attr_delete_map_void(map_privates);
	map_privates = NULL;
	attr_delete_map(map_form);
	attr_delete_map_rev(map_form_rev);
	attr_delete_map(map_forms);
	attr_delete_map_rev(map_rev_forms);
	attr_delete_map(map_inputs);
	attr_delete_map(map_nodelist);
//	attr_delete_map_rev(map_rev_nodelist);

	attr_delete_map(map_form_elements);
	attr_delete_map_rev(map_form_elements_rev);

	attr_delete_map(map_csses);
	attr_delete_map_rev(map_rev_csses);

//	if (js_module_init_ok)
//		spidermonkey_runtime_release();
}

int
el_js_module_set_import_meta(JSContext *ctx, JSValueConst func_val, JS_BOOL use_realpath, JS_BOOL is_main)
{
	JSModuleDef *m;
	//char buf[PATH_MAX + 16];
	JSValue meta_obj;
	//JSAtom module_name_atom;
	//const char *module_name;

	assert(JS_VALUE_GET_TAG(func_val) == JS_TAG_MODULE);
	m = JS_VALUE_GET_PTR(func_val);

#if 0
	module_name_atom = JS_GetModuleName(ctx, m);
	module_name = JS_AtomToCString(ctx, module_name_atom);
	JS_FreeAtom(ctx, module_name_atom);

	if (!module_name) {
		return -1;
	}

	if (!strchr(module_name, ':')) {
		strcpy(buf, "file://");
#if !defined(_WIN32)
		/* realpath() cannot be used with modules compiled with qjsc
		 because the corresponding module source code is not
		 necessarily present */

		if (use_realpath) {
			char *res = realpath(module_name, buf + strlen(buf));

			if (!res) {
				JS_ThrowTypeError(ctx, "realpath failure");
				JS_FreeCString(ctx, module_name);
				return -1;
			}
		} else
#endif
		{
			pstrcat(buf, sizeof(buf), module_name);
		}
	} else {
		pstrcpy(buf, sizeof(buf), module_name);
	}
	JS_FreeCString(ctx, module_name);
#endif // 0

	meta_obj = JS_GetImportMeta(ctx, m);

	if (JS_IsException(meta_obj)) {
		return -1;
	}
#if 0
	JS_DefinePropertyValueStr(ctx, meta_obj, "url", JS_NewString(ctx, buf), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, meta_obj, "main", JS_NewBool(ctx, is_main), JS_PROP_C_W_E);
#endif
	JS_FreeValue(ctx, meta_obj);

	return 0;
}

JSModuleDef *
el_js_module_loader(JSContext *ctx, const char *module_name, void *opaque)
{
	JSModuleDef *m = NULL;

	if (!strcmp(module_name, "a")) {
		/* compile the module */
		JSValue func_val = JS_Eval(ctx, (char *)fetch_js, fetch_js_len, module_name, JS_EVAL_TYPE_MODULE);

		if (JS_IsException(func_val)) {
			return NULL;
		}
		/* XXX: could propagate the exception */
		el_js_module_set_import_meta(ctx, func_val, 1, 0);
		/* the module is already referenced, so we must free it */
		m = JS_VALUE_GET_PTR(func_val);
		JS_FreeValue(ctx, func_val);
	}
	return m;
}

void *
quickjs_get_interpreter(struct ecmascript_interpreter *interpreter)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSContext *ctx;
//	JSObject *console_obj, *document_obj, /* *forms_obj,*/ *history_obj, *location_obj,
//	         *statusbar_obj, *menubar_obj, *navigator_obj, *localstorage_obj, *screen_obj;

	assert(interpreter);
//	if (!js_module_init_ok) return NULL;

#ifdef CONFIG_DEBUG
	interpreter->rt = JS_NewRuntime2(&el_quickjs_mf, NULL);
#else
	interpreter->rt = JS_NewRuntime();
#endif
	if (!interpreter->rt) {
		return NULL;
	}
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	JS_SetMemoryLimit(interpreter->rt, 64 * 1024 * 1024);
	JS_SetGCThreshold(interpreter->rt, 16 * 1024 * 1024);

	ctx = JS_NewContext(interpreter->rt);

	if (!ctx) {
		JS_FreeRuntime(interpreter->rt);
		return NULL;
	}

	interpreter->backend_data = ctx;
	JS_SetContextOpaque(ctx, interpreter);

//	JS::SetWarningReporter(ctx, error_reporter);

	JS_SetInterruptHandler(interpreter->rt, js_heartbeat_callback, interpreter);
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
	js_xhr_init(ctx);
	js_event_init(ctx);
	js_keyboardEvent_init(ctx);
	js_messageEvent_init(ctx);
	js_customEvent_init(ctx);
	js_url_init(ctx);
	js_urlSearchParams_init(ctx);
	js_domparser_init(ctx);

	interpreter->document_obj = getDocument(ctx, document->dom);

	JS_SetModuleLoaderFunc(interpreter->rt, NULL, el_js_module_loader, NULL);
	JSModuleDef *m = el_js_module_loader(ctx, "a", NULL);

	JSValue r = JS_Eval(ctx, "import {fetch,Headers,Request,Response} from 'a';", sizeof("import {fetch,Headers,Request,Response} from 'a';") - 1,
	"top", 0);

	JS_FreeValue(ctx, r);

	return ctx;
}

void
quickjs_put_interpreter(struct ecmascript_interpreter *interpreter)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSContext *ctx;

	assert(interpreter);

	ctx = (JSContext *)interpreter->backend_data;

	JS_FreeContext(ctx);
	JS_FreeRuntime(interpreter->rt);
	interpreter->backend_data = NULL;
}

static void
js_dump_obj(JSContext *ctx, struct string *f, JSValueConst val)
{
	const char *str;

	str = JS_ToCString(ctx, val);

	if (str) {
		add_to_string(f, str);
		add_char_to_string(f, '\n');
		JS_FreeCString(ctx, str);
	} else {
		add_to_string(f, "[exception]\n");
	}
}

static void
js_dump_error1(JSContext *ctx, struct string *f, JSValueConst exception_val)
{
	JSValue val;
	bool is_error;

	is_error = JS_IsError(ctx, exception_val);
	js_dump_obj(ctx, f, exception_val);

	if (is_error) {
		val = JS_GetPropertyStr(ctx, exception_val, "stack");

		if (!JS_IsUndefined(val)) {
			js_dump_obj(ctx, f, val);
		}
		JS_FreeValue(ctx, val);
	}
}

static void
js_dump_error(JSContext *ctx, struct string *f)
{
	JSValue exception_val;

	exception_val = JS_GetException(ctx);
	js_dump_error1(ctx, f, exception_val);
	JS_FreeValue(ctx, exception_val);
}

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

void
quickjs_eval(struct ecmascript_interpreter *interpreter,
                  struct string *code, struct string *ret)
{
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
}

void
quickjs_call_function(struct ecmascript_interpreter *interpreter,
                  JSValueConst fun, struct string *ret)
{
	JSContext *ctx;

	assert(interpreter);
//	if (!js_module_init_ok) {
//		return;
//	}
	ctx = (JSContext *)interpreter->backend_data;

	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = ret;

	JSValue global_object = JS_GetGlobalObject(ctx);
	REF_JS(global_object);

	JSValue r = JS_Call(ctx, fun, global_object, 0, NULL);
	JS_FreeValue(ctx, global_object);

	done_heartbeat(interpreter->heartbeat);

	if (JS_IsException(r)) {
		error_reporter(interpreter, ctx);
	}
}

char *
quickjs_eval_stringback(struct ecmascript_interpreter *interpreter,
			     struct string *code)
{
	JSContext *ctx;

	assert(interpreter);
//	if (!js_module_init_ok) {
//		return;
//	}
	ctx = (JSContext *)interpreter->backend_data;
	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = NULL;
	JSValue r = JS_Eval(ctx, code->source, code->length, "", 0);
	done_heartbeat(interpreter->heartbeat);

	if (JS_IsNull(r)) {
		return NULL;
	}

	if (JS_IsException(r)) {
		error_reporter(interpreter, ctx);
	}

	const char *str;
	char *string;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, r);

	if (!str) {
		return NULL;
	}
	string = stracpy(str);
	JS_FreeCString(ctx, str);

	return string;
}

int
quickjs_eval_boolback(struct ecmascript_interpreter *interpreter,
			   struct string *code)
{
	JSContext *ctx;

	assert(interpreter);
//	if (!js_module_init_ok) {
//		return;
//	}
	ctx = (JSContext *)interpreter->backend_data;
	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = NULL;
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
}

struct module quickjs_module = struct_module(
	/* name: */		N_("QuickJS"),
	/* options: */		NULL,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		quickjs_init,
	/* done: */		quickjs_done
);
