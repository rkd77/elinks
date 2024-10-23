/* The SpiderMonkey ECMAScript backend. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/spidermonkey/util.h"

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
#include "js/spidermonkey.h"
#include "js/spidermonkey/console.h"
#include "js/spidermonkey/customevent.h"
#include "js/spidermonkey/document.h"
#include "js/spidermonkey/domparser.h"
#include "js/spidermonkey/event.h"
#include "js/spidermonkey/form.h"
#include "js/spidermonkey/heartbeat.h"
#include "js/spidermonkey/history.h"
#include "js/spidermonkey/keyboard.h"
#include "js/spidermonkey/location.h"
#include "js/spidermonkey/localstorage.h"
#include "js/spidermonkey/message.h"
#include "js/spidermonkey/navigator.h"
#include "js/spidermonkey/node.h"
#include "js/spidermonkey/screen.h"
#include "js/spidermonkey/unibar.h"
#include "js/spidermonkey/url.h"
#include "js/spidermonkey/urlsearchparams.h"
#include "js/spidermonkey/window.h"
#include "js/spidermonkey/xhr.h"
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
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"

#include <jsapi.h>
#include <js/CompilationAndEvaluation.h>
#include <js/Printf.h>
#include <js/SourceText.h>
#include <js/Warnings.h>
#include <js/Modules.h>

#include <map>

/*** Global methods */

#include "js/fetch.h"

/* TODO? Are there any which need to be implemented? */

static int js_module_init_ok;

std::map<void *, bool> interps;

static void
error_reporter(JSContext *ctx, JSErrorReport *report)
{
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
		return;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct session *ses = interpreter->vs->doc_view->session;
	struct terminal *term;
	struct string msg;
	char *ptr;
	size_t size;
	FILE *f;

	assert(interpreter && interpreter->vs && interpreter->vs->doc_view
	       && ses && ses->tab);
	if_assert_failed goto reported;

	term = ses->tab->term;

#ifdef CONFIG_LEDS
	set_led_value(ses->status.ecmascript_led, 'J');
#endif

	if (!get_opt_bool("ecmascript.error_reporting", ses))
		goto reported;

	f = open_memstream(&ptr, &size);

	if (f) {
		JS::PrintError(f, report, true/*reportWarnings*/);
		fclose(f);

		if (!init_string(&msg)) {
			free(ptr);
		} else {
			add_to_string(&msg,
			_("A script embedded in the current document raised the following:\n", term));
			add_bytes_to_string(&msg, ptr, size);
			free(ptr);

			info_box(term, MSGBOX_FREE_TEXT, N_("JavaScript Error"), ALIGN_CENTER, msg.source);
		}
	}
reported:
	JS_ClearPendingException(ctx);
}

static int
change_hook_spidermonkey(struct session *ses, struct option *current, struct option *changed)
{
	spidermonkey_memory_limit = get_opt_long("ecmascript.spidermonkey.memory_limit", ses);

	return 0;
}

static void
spidermonkey_init(struct module *module)
{
	static const struct change_hook_info spidermonkey_change_hooks[] = {
		{ "ecmascript.spidermonkey.memory_limit", change_hook_spidermonkey },
		{ NULL,	NULL },
	};
	register_change_hooks(spidermonkey_change_hooks);
	spidermonkey_memory_limit = get_opt_long("ecmascript.spidermonkey.memory_limit", NULL);

	js_module_init_ok = spidermonkey_runtime_addref();
}

static void
spidermonkey_done(struct module *xxx)
{
	if (js_module_init_ok) {
		spidermonkey_release_all_runtimes();
	}
}

static const char *
get_name_spidermonkey(struct module *module)
{
	static char spidermonkey_version[32];

	snprintf(spidermonkey_version, 31, "mozjs %s", JS_GetImplementationVersion());
	return spidermonkey_version;
}

static JSObject*
CompileExampleModule(JSContext* cx, const char* filename, const char* code, size_t len)
{
	JS::CompileOptions options(cx);
	options.setFileAndLine(filename, 1);

	JS::SourceText<mozilla::Utf8Unit> source;

	if (!len) {
		len = strlen(code);
	}

	if (!source.init(cx, code, len, JS::SourceOwnership::Borrowed)) {
		return nullptr;
	}
	// Compile the module source to bytecode.
	//
	// NOTE: This generates a JSObject instead of a JSScript. This contains
	// additional metadata to resolve imports/exports. This object should not be
	// exposed to other JS code or unexpected behaviour may occur.

	return JS::CompileModule(cx, options, source);
}

// Maintain a registry of imported modules. The ResolveHook may be called
// multiple times for the same specifier and we need to return the same compiled
// module.
//
// NOTE: This example assumes only one JSContext/GlobalObject is used, but in
// general the registry needs to be distinct for each GlobalObject.
std::map<JSObject *, JS::PersistentRootedObject> moduleRegistry;

// Callback for embedding to provide modules for import statements. This example
// hardcodes sources, but an embedding would normally load files here.
static JSObject*
ExampleResolveHook(JSContext* cx, JS::HandleValue modulePrivate, JS::HandleObject moduleRequest)
{
	// Extract module specifier string.
	JS::Rooted<JSString*> specifierString(cx, JS::GetModuleRequestSpecifier(cx, moduleRequest));

	if (!specifierString) {
		return nullptr;
	}
	// Convert specifier to a std::u16char for simplicity.
	JS::UniqueTwoByteChars specChars(JS_CopyStringCharsZ(cx, specifierString));

	if (!specChars) {
		return nullptr;
	}
	std::u16string filename(specChars.get());
	JSObject *global = JS::CurrentGlobalOrNull(cx);

	// If we already resolved before, return same module.
	auto search = moduleRegistry.find(global);

	if (search != moduleRegistry.end()) {
		return search->second;
	}
	JS::RootedObject mod(cx);

	if (filename == u"a") {
		mod = CompileExampleModule(cx, "a", (const char *)fetch_js, (size_t)fetch_js_len);

		if (!mod) {
			return nullptr;
		}
	}
	// Register result in table.

	if (mod) {
		moduleRegistry[global] = JS::PersistentRootedObject(cx, mod);
		return mod;
	}
	JS_ReportErrorASCII(cx, "Cannot resolve import specifier");

	return nullptr;
}


void *
spidermonkey_get_interpreter(struct ecmascript_interpreter *interpreter)
{
	JSContext *ctx;
	JSObject *console_obj, *document_obj, /* *forms_obj,*/ *history_obj,
	         *statusbar_obj, *menubar_obj, *navigator_obj, *node_obj, *localstorage_obj, *screen_obj,
	         *xhr_obj, *event_obj, *keyboardEvent_obj, *messageEvent_obj, *customEvent_obj,
	         *url_obj, *urlSearchParams_obj, *domparser_obj;

	assert(interpreter);
	if (!js_module_init_ok) return NULL;

	ctx = main_ctx;

	if (!ctx) {
		return nullptr;
	}
	interps[(void *)interpreter] = true;

	interpreter->backend_data = ctx;
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	// JS_SetContextPrivate(ctx, interpreter);

	JS::SetWarningReporter(ctx, error_reporter);

	JS_AddInterruptCallback(ctx, heartbeat_callback);
	JS::RealmOptions options;
	JS::RootedObject global(ctx);
	JS::RootedObject mod(ctx);
	JS::CompileOptions copt(ctx);
	JS::SourceText<mozilla::Utf8Unit> srcBuf;
	JS::RootedValue rval(ctx);

	JS::Heap<JSObject*> *window_obj = new JS::Heap<JSObject*>(JS_NewGlobalObject(ctx, &window_class, NULL, JS::FireOnNewGlobalHook, options));

	global = window_obj->get();

	if (!global) {
		goto release_and_fail;
	}
	interpreter->ar = new JSAutoRealm(ctx, global);
	interpreter->ac = window_obj;

	if (!JS::InitRealmStandardClasses(ctx)) {
		goto release_and_fail;
	}

	if (!JS_DefineProperties(ctx, global, window_props)) {
		goto release_and_fail;
	}

	if (!spidermonkey_DefineFunctions(ctx, global, window_funcs)) {
		goto release_and_fail;
	}
	//JS_SetPrivate(window_obj, interpreter); /* to @window_class */

	document_obj = spidermonkey_InitClass(ctx, global, NULL,
					      &document_class, NULL, 0,
					      document_props,
					      document_funcs,
					      NULL, NULL, "document");
	if (!document_obj) {
		goto release_and_fail;
	}

	if (!initDocument(ctx, interpreter, document_obj, document->dom)) {
		goto release_and_fail;
	}

	interpreter->document_obj = document_obj;

/*
	forms_obj = spidermonkey_InitClass(ctx, document_obj, NULL,
					   &forms_class, NULL, 0,
					   forms_props,
					   forms_funcs,
					   NULL, NULL);
	if (!forms_obj) {
		goto release_and_fail;
	}
*/

	history_obj = spidermonkey_InitClass(ctx, global, NULL,
					     &history_class, NULL, 0,
					     (JSPropertySpec *) NULL,
					     history_funcs,
					     NULL, NULL, "history");
	if (!history_obj) {
		goto release_and_fail;
	}
	screen_obj = spidermonkey_InitClass(ctx, global, NULL,
					      &screen_class, NULL, 0,
					      screen_props,
					      NULL,
					      NULL, NULL, "screen");

	if (!screen_obj) {
		goto release_and_fail;
	}

	menubar_obj = JS_InitClass(ctx, global, &menubar_class, nullptr,
				   "menubar", NULL, 0,
				   unibar_props, NULL,
				   NULL, NULL);
	if (!menubar_obj) {
		goto release_and_fail;
	}
	JS::SetReservedSlot(menubar_obj, 0, JS::PrivateValue((char *)"t")); /* to @menubar_class */

	statusbar_obj = JS_InitClass(ctx, global, &statusbar_class, nullptr,
				     "statusbar", NULL, 0,
				     unibar_props, NULL,
				     NULL, NULL);
	if (!statusbar_obj) {
		goto release_and_fail;
	}
	JS::SetReservedSlot(statusbar_obj, 0, JS::PrivateValue((char *)"s")); /* to @statusbar_class */

	navigator_obj = JS_InitClass(ctx, global, &navigator_class, nullptr,
				     "navigator", NULL, 0,
				     navigator_props, NULL,
				     NULL, NULL);
	if (!navigator_obj) {
		goto release_and_fail;
	}

	console_obj = spidermonkey_InitClass(ctx, global, NULL,
					      &console_class, NULL, 0,
					      nullptr,
					      console_funcs,
					      NULL, NULL, "console");
	if (!console_obj) {
		goto release_and_fail;
	}

	localstorage_obj = spidermonkey_InitClass(ctx, global, NULL,
					      &localstorage_class, NULL, 0,
					      nullptr,
					      localstorage_funcs,
					      NULL, NULL, "localStorage");
	if (!localstorage_obj) {
		goto release_and_fail;
	}

	xhr_obj = spidermonkey_InitClass(ctx, global, NULL,
					&xhr_class, xhr_constructor, 0,
					xhr_props,
					xhr_funcs,
					xhr_static_props, NULL, "XMLHttpRequest");

	if (!xhr_obj) {
		goto release_and_fail;
	}

	event_obj = spidermonkey_InitClass(ctx, global, NULL,
					&event_class, event_constructor, 0,
					event_props,
					event_funcs,
					NULL, NULL, "Event");

	if (!event_obj) {
		goto release_and_fail;
	}

	keyboardEvent_obj = spidermonkey_InitClass(ctx, global, NULL,
					&keyboardEvent_class, keyboardEvent_constructor, 0,
					keyboardEvent_props,
					keyboardEvent_funcs,
					NULL, NULL, "KeyboardEvent");

	if (!keyboardEvent_obj) {
		goto release_and_fail;
	}

	messageEvent_obj = spidermonkey_InitClass(ctx, global, NULL,
					&messageEvent_class, messageEvent_constructor, 0,
					messageEvent_props,
					messageEvent_funcs,
					NULL, NULL, "MessageEvent");

	if (!messageEvent_obj) {
		goto release_and_fail;
	}

	customEvent_obj = spidermonkey_InitClass(ctx, global, NULL,
					&customEvent_class, customEvent_constructor, 0,
					customEvent_props,
					customEvent_funcs,
					NULL, NULL, "CustomEvent");

	if (!customEvent_obj) {
		goto release_and_fail;
	}

	url_obj = spidermonkey_InitClass(ctx, global, NULL,
					&url_class, url_constructor, 0,
					url_props,
					url_funcs,
					NULL, NULL, "URL");

	if (!url_obj) {
		goto release_and_fail;
	}

	urlSearchParams_obj = spidermonkey_InitClass(ctx, global, NULL,
					&urlSearchParams_class, urlSearchParams_constructor, 0,
					urlSearchParams_props,
					urlSearchParams_funcs,
					NULL, NULL, "URLSearchParams");

	if (!urlSearchParams_obj) {
		goto release_and_fail;
	}

	domparser_obj = spidermonkey_InitClass(ctx, global, NULL,
					&domparser_class, domparser_constructor, 0,
					NULL,
					domparser_funcs,
					NULL, NULL, "DOMParser");

	if (!domparser_obj) {
		goto release_and_fail;
	}

	node_obj = spidermonkey_InitClass(ctx, global, NULL,
					&node_class, node_constructor, 0,
					NULL,
					NULL,
					node_static_props, NULL, "Node");

	if (!node_obj) {
		goto release_and_fail;
	}

#if 1
	// Register a hook in order to provide modules
	JS::SetModuleResolveHook(JS_GetRuntime(ctx), ExampleResolveHook);
	mod = CompileExampleModule(ctx, "top", "import {fetch,Headers,Request,Response} from 'a';", 0);

	if (!mod) {
		goto release_and_fail;
	}

	// Resolve imports by loading and compiling additional scripts.
	if (!JS::ModuleLink(ctx, mod)) {
		goto release_and_fail;
	}
	// Result value, used for top-level await.

	// Execute the module bytecode.
	if (!JS::ModuleEvaluate(ctx, mod, &rval)) {
		goto release_and_fail;
	}
#endif
	JS::SetRealmPrivate(js::GetContextRealm(ctx), interpreter);

	return ctx;

release_and_fail:
	spidermonkey_put_interpreter(interpreter);
	return NULL;
}

void
spidermonkey_put_interpreter(struct ecmascript_interpreter *interpreter)
{
	assert(interpreter);
	if (!js_module_init_ok) return;

	delete interpreter->ar;
	delete interpreter->ac;

	interpreter->backend_data = NULL;
	interpreter->ac = nullptr;
	interpreter->ar = nullptr;
	done_heartbeat(interpreter->heartbeat);

	interps.erase((void *)interpreter);
}

void
spidermonkey_check_for_exception(JSContext *ctx) {
	if (JS_IsExceptionPending(ctx))
	{
		JS::RootedValue exception(ctx);
	         if(JS_GetPendingException(ctx,&exception) && exception.isObject()) {
			JS::AutoSaveExceptionState savedExc(ctx);
			JS::Rooted<JSObject*> exceptionObject(ctx, &exception.toObject());
			JSErrorReport *report = JS_ErrorFromException(ctx,exceptionObject);
			if(report) {
				if (report->lineno>0) {
					/* Somehow the reporter alway reports first error
					 * Undefined and with line 0. Let's filter this. */
					/* Optional printing javascript error to file */
					//FILE *f = fopen("js.err","a");
					//PrintError(f, report->message(), report, true);
					/* Send the error to the tui */
					error_reporter(ctx, report);
					//DBG("file: %s",report->filename);
					//DBG("file: %s",report->message());
					//DBG("file: %d",(int) report->lineno);
				}
			}
			//JS_ClearPendingException(ctx);
		}
		/* This absorbs all following exceptions
		 * probably not the 100% correct solution
		 * to the javascript error handling but
		 * at least there isn't too much click-bait
		 * on each site with javascript enabled */
		JS_ClearPendingException(ctx);
	}
}

void
spidermonkey_eval(struct ecmascript_interpreter *interpreter,
                  struct string *code, struct string *ret)
{
	JSContext *ctx;
	JS::Value rval;

	assert(interpreter);
	if (!js_module_init_ok) {
		return;
	}
	ctx = (JSContext *)interpreter->backend_data;
	JSAutoRealm ar(ctx, (JSObject *)interpreter->ac->get());

	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = ret;

	JS::RootedObject cg(ctx, JS::CurrentGlobalOrNull(ctx));
	JS::RootedValue r_val(ctx, rval);
	JS::CompileOptions options(ctx);

	JS::SourceText<mozilla::Utf8Unit> srcBuf;
	if (!srcBuf.init(ctx, code->source, code->length, JS::SourceOwnership::Borrowed)) {
		return;
	}
	JS::Evaluate(ctx, options, srcBuf, &r_val);

	spidermonkey_check_for_exception(ctx);

	done_heartbeat(interpreter->heartbeat);
}

void
spidermonkey_call_function(struct ecmascript_interpreter *interpreter,
                  JS::HandleValue fun, struct string *ret)
{
	JSContext *ctx;
	JS::Value rval;

	assert(interpreter);
	if (!js_module_init_ok) {
		return;
	}
	ctx = (JSContext *)interpreter->backend_data;
	JSAutoRealm ar(ctx, (JSObject *)interpreter->ac->get());

	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = ret;

	JS::RootedValue r_val(ctx, rval);
	JS::RootedObject cg(ctx, JS::CurrentGlobalOrNull(ctx));
	JS_CallFunctionValue(ctx, cg, fun, JS::HandleValueArray::empty(), &r_val);
	done_heartbeat(interpreter->heartbeat);
}


char *
spidermonkey_eval_stringback(struct ecmascript_interpreter *interpreter,
			     struct string *code)
{
	bool ret;
	JSContext *ctx;
	JS::Value rval;
	char *result = NULL;

	assert(interpreter);
	if (!js_module_init_ok) return NULL;
	ctx = (JSContext *)interpreter->backend_data;
	interpreter->ret = NULL;
	interpreter->heartbeat = add_heartbeat(interpreter);

	JSAutoRealm ar(ctx, (JSObject *)interpreter->ac->get());

	JS::RootedObject cg(ctx, JS::CurrentGlobalOrNull(ctx));
	JS::RootedValue r_rval(ctx, rval);
	JS::CompileOptions options(ctx);

//	options.setIntroductionType("js shell load")
//	.setUTF8(true)
//	.setCompileAndGo(true)
//	.setNoScriptRval(true);

	JS::SourceText<mozilla::Utf8Unit> srcBuf;
	if (!srcBuf.init(ctx, code->source, code->length, JS::SourceOwnership::Borrowed)) {
		return NULL;
	}
	ret = JS::Evaluate(ctx, options, srcBuf, &r_rval);
	done_heartbeat(interpreter->heartbeat);

	if (ret == false) {
		result = NULL;
	}
	else if (r_rval.isNullOrUndefined()) {
		/* Undefined value. */
		result = NULL;
	} else {
		result = jsval_to_string(ctx, r_rval);
	}

	return result;
}

int
spidermonkey_eval_boolback(struct ecmascript_interpreter *interpreter,
			   struct string *code)
{
	JSContext *ctx;
	JS::Value rval;
	int ret;
	int result = 0;

	assert(interpreter);
	if (!js_module_init_ok) return 0;
	ctx = (JSContext *)interpreter->backend_data;
	interpreter->ret = NULL;

	JSAutoRealm ar(ctx, (JSObject *)interpreter->ac->get());

	JS::CompileOptions options(ctx);
	JS::RootedObjectVector ag(ctx);

	JS::SourceText<mozilla::Utf8Unit> srcBuf;
	if (!srcBuf.init(ctx, code->source, code->length, JS::SourceOwnership::Borrowed)) {
		return -1;
	}

	JSFunction *funs = JS::CompileFunction(ctx, ag, options, "aaa", 0, nullptr, srcBuf);
	if (!funs) {
		return -1;
	};

	interpreter->heartbeat = add_heartbeat(interpreter);
	JS::RootedValue r_val(ctx, rval);
	JS::RootedObject cg(ctx, JS::CurrentGlobalOrNull(ctx));
	JS::RootedFunction fun(ctx, funs);
	ret = JS_CallFunction(ctx, cg, fun, JS::HandleValueArray::empty(), &r_val);
	done_heartbeat(interpreter->heartbeat);

	if (ret == 2) { /* onClick="history.back()" */
		result = 0;
	}
	else if (ret == false) {
		result = -1;
	}
	else if (r_val.isUndefined()) {
		/* Undefined value. */
		result = -1;
	} else {
		result = r_val.toBoolean();
	}

	return result;
}

static union option_info spidermonkey_options[] = {
	INIT_OPT_TREE("ecmascript", N_("Spidermonkey"),
		"spidermonkey", OPT_ZERO,
		N_("Options specific to Spidermonkey.")),

	INIT_OPT_LONG("ecmascript.spidermonkey", N_("Memory limit"),
		"memory_limit", OPT_ZERO, 0, LONG_MAX, 128 * 1024 * 1024,
		N_("Runtime memory limit in bytes.")),

	NULL_OPTION_INFO,
};

struct module spidermonkey_module = struct_module(
	/* name: */		N_("SpiderMonkey"),
	/* options: */		spidermonkey_options,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		spidermonkey_init,
	/* done: */		spidermonkey_done,
	/* getname: */	get_name_spidermonkey
);
