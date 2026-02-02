/* Base ECMAScript file. Mostly a proxy for specific library backends. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "elinks.h"

#include "js/libdom/dom.h"

#include "config/home.h"
#include "config/options.h"
#include "dialogs/status.h"
#include "document/document.h"
#include "document/libdom/doc.h"
#include "document/libdom/mapa.h"
#include "document/libdom/renderer.h"
#include "document/libdom/renderer2.h"
#include "document/renderer.h"
#include "document/view.h"
#include "js/ecmascript.h"
#include "js/ecmascript-c.h"
#include "js/libdom/parse.h"
#ifdef CONFIG_MUJS
#include "js/mujs.h"
#else
#ifdef CONFIG_QUICKJS
#include "js/quickjs.h"
#include "js/quickjs/document.h"
#else
#include "js/spidermonkey.h"
#endif
#endif
#include "js/timer.h"
#include "intl/libintl.h"
#include "main/module.h"
#include "main/main.h"
#include "main/select.h"
#include "main/timer.h"
#include "osdep/osdep.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/conv.h"
#include "util/memcount.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/view.h" /* current_frame() */
#include "viewer/text/form.h" /* <-ecmascript_reset_state() */
#include "viewer/text/vs.h"

#ifdef CONFIG_LIBCURL
#include <curl/curl.h>
#endif

#undef max
#undef min

/* TODO: We should have some kind of ACL for the scripts - i.e. ability to
 * disallow the scripts to open new windows (or so that the windows are always
 * directed to tabs, this particular option would be a tristate), disallow
 * messing with your menubar/statusbar visibility, disallow changing the
 * statusbar content etc. --pasky */

static union option_info ecmascript_options[] = {
	INIT_OPT_TREE("", N_("ECMAScript"),
		"ecmascript", OPT_ZERO,
		N_("ECMAScript options.")),

	INIT_OPT_BOOL("ecmascript", N_("Allow XHR requests to local files"),
		"allow_xhr_file", OPT_ZERO, 0,
		N_("Whether to allow XHR requests to local files.")),

	INIT_OPT_BOOL("ecmascript", N_("Pop-up window blocking"),
		"block_window_opening", OPT_ZERO, 0,
		N_("Whether to disallow scripts to open new windows or tabs.")),

	INIT_OPT_BOOL("ecmascript", N_("Check integrity"),
		"check_integrity", OPT_ZERO, 1,
		N_("Whether to check integrity. It can have negative impact on performance.")),

	INIT_OPT_BOOL("ecmascript", N_("Enable"),
		"enable", OPT_ZERO, 0,
		N_("Whether to run those scripts inside of documents.")),

	INIT_OPT_BOOL("ecmascript", N_("Console log"),
		"enable_console_log", OPT_ZERO, 0,
		N_("When enabled logs will be appended to ~/.config/elinks/console.log.")),

	INIT_OPT_BOOL("ecmascript", N_("Script error reporting"),
		"error_reporting", OPT_ZERO, 0,
		N_("Open a message box when a script reports an error.")),

	INIT_OPT_BOOL("ecmascript", N_("Ignore <noscript> content"),
		"ignore_noscript", OPT_ZERO, 0,
		N_("Whether to ignore content enclosed by the <noscript> tag "
		"when ECMAScript is enabled.")),

	INIT_OPT_INT("ecmascript", N_("Maximum execution time"),
		"max_exec_time", OPT_ZERO, 1, 3600, 5,
		N_("Maximum execution time in seconds for a script.")),

	NULL_OPTION_INFO,
};

int interpreter_count;

static INIT_LIST_OF(struct string_list_item, allowed_urls);
static INIT_LIST_OF(struct string_list_item, disallowed_urls);
static INIT_LIST_OF(struct ecmascript_interpreter, ecmascript_interpreters);

char *console_error_filename;
char *console_log_filename;
char *console_warn_filename;

char *local_storage_filename;

int local_storage_ready;

static void ecmascript_request(void *val);

static int
is_prefix(char *prefix, char *url, int dl)
{
	ELOG
	return memcmp(prefix, url, dl);
}

static void
read_url_list(void)
{
	ELOG
	char *xdg_config_home = get_xdg_config_home();
	char line[4096];
	char *filename;
	FILE *f;

	if (!xdg_config_home) {
		return;
	}

	filename = straconcat(xdg_config_home, ALLOWED_ECMASCRIPT_URL_PREFIXES, NULL);

	if (filename) {

		f = fopen(filename, "r");

		if (f) {
			while (fgets(line, 4096, f)) {
				add_to_string_list(&allowed_urls, line, strlen(line) - 1);
			}
			fclose(f);
		}
		mem_free(filename);
	}

	filename = straconcat(xdg_config_home, DISALLOWED_ECMASCRIPT_URL_PREFIXES, NULL);

	if (filename) {

		f = fopen(filename, "r");

		if (f) {
			while (fgets(line, 4096, f)) {
				add_to_string_list(&disallowed_urls, line, strlen(line) - 1);
			}
			fclose(f);
		}
		mem_free(filename);
	}
}

int ecmascript_enabled;

int
is_ecmascript_enabled_for_interpreter(struct ecmascript_interpreter *interpreter)
{
	struct uri *uri = (interpreter && interpreter->vs && interpreter->vs->doc_view
		&& interpreter->vs->doc_view->document) ? interpreter->vs->doc_view->document->uri
		: NULL;

	return is_ecmascript_enabled_for_uri(uri);
}

int
is_ecmascript_enabled_for_uri(struct uri *uri)
{
	ELOG
	struct string_list_item *item;
	char *url;

	if (!ecmascript_enabled || !get_opt_bool("ecmascript.enable", NULL) || !uri) {
		return 0;
	}

	if (list_empty(allowed_urls) && list_empty(disallowed_urls)) {
		return 1;
	}

	url = get_uri_string(uri, URI_PUBLIC);
	if (!url) {
		return 0;
	}

	foreach(item, disallowed_urls) {
		struct string *string = &item->string;

		if (string->length <= 0) {
			continue;
		}
		if (!is_prefix(string->source, url, string->length)) {
			mem_free(url);
			move_to_top_of_list(disallowed_urls, item);
			return 0;
		}
	}

	foreach(item, allowed_urls) {
		struct string *string = &item->string;

		if (string->length <= 0) {
			continue;
		}
		if (!is_prefix(string->source, url, string->length)) {
			mem_free(url);
			move_to_top_of_list(allowed_urls, item);
			return 1;
		}
	}

	mem_free(url);
	return list_empty(allowed_urls);
}

struct ecmascript_interpreter *
ecmascript_get_interpreter(struct view_state *vs)
{
	ELOG
	struct ecmascript_interpreter *interpreter;

	assert(vs);

	interpreter = (struct ecmascript_interpreter *)mem_calloc(1, sizeof(*interpreter));
	if (!interpreter)
		return NULL;

	++interpreter_count;

	interpreter->vs = vs;
	interpreter->vs->ecmascript_fragile = 0;
	init_list(interpreter->onload_snippets);
	/* The following backend call reads interpreter->vs.  */
	if (
#ifdef CONFIG_MUJS
	    !mujs_get_interpreter(interpreter)
#elif defined(CONFIG_QUICKJS)
	    !quickjs_get_interpreter(interpreter)
#else
	    !spidermonkey_get_interpreter(interpreter)
#endif
	    ) {
		/* Undo what was done above.  */
		interpreter->vs->ecmascript_fragile = 1;
		mem_free(interpreter);
		--interpreter_count;
		return NULL;
	}

	(void)init_string(&interpreter->code);
	init_list(interpreter->writecode);
	interpreter->current_writecode = (struct ecmascript_string_list_item *)interpreter->writecode.next;
	init_list(interpreter->timeouts);
	add_to_list(ecmascript_interpreters, interpreter);
#ifdef CONFIG_QUICKJS
	interpreter->request_func = JS_NULL;
#endif
	interpreter->time_origin = get_time();
	install_timer(&interpreter->ani, REQUEST_ANIMATION_FRAME, ecmascript_request, interpreter);

	return interpreter;
}

static void
delayed_reload(void *data)
{
	ELOG
	struct delayed_rel *rel = (struct delayed_rel *)data;
	assert(rel);
	struct session *ses = rel->ses;
	struct document *document = rel->document;

	//object_unlock(document); // better a memleak than a segfault

	reset_document(document);
	document->links_sorted = 0;
	dump_xhtml(rel->cached, document, 1 + rel->was_write);

	if (!ses->doc_view->vs->plain && document->options.html_compress_empty_lines) {
		compress_empty_lines(document);
	}

	sort_links(document);
	draw_formatted(ses, 2);
	load_common(ses);
	mem_free(rel);
}

static void
run_jobs(void *data)
{
	ELOG
#ifdef CONFIG_ECMASCRIPT_SMJS
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)data;
	js::RunJobs((JSContext *)interpreter->backend_data);
#endif

#ifdef CONFIG_QUICKJS
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)data;
	JSContext *ctx = (JSContext *)interpreter->backend_data;
	JSContext *ctx1;
	int err;
	/* execute the pending jobs */
	for(;;) {
		err = JS_ExecutePendingJob(JS_GetRuntime(ctx), &ctx1);

		if (err <= 0) {
			break;
		}
	}
#endif
}

void
check_for_rerender(struct ecmascript_interpreter *interpreter, const char* text)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %s %d\n", __FILE__, __FUNCTION__, text, interpreter->changed);
#endif
	run_jobs(interpreter);

	if (interpreter->changed && !program.testjs) {
		struct document_view *doc_view = interpreter->vs->doc_view;
		struct document *document = doc_view->document;
		struct session *ses = doc_view->session;
		struct cache_entry *cached = document->cached;
#ifdef CONFIG_ECMASCRIPT_SMJS
		if (interpreter->document_obj) {
			dom_html_document *doc = JS::GetMaybePtrFromReservedSlot<dom_html_document>((JSObject *)interpreter->document_obj, 0);

			if (doc) {
				document->dom = doc;
			}
		}
#endif
#ifdef CONFIG_QUICKJS
		if (JS_IsObject(interpreter->document_obj)) {
			dom_html_document *doc = js_doc_getopaque(interpreter->document_obj);

			if (doc) {
				document->dom = doc;
			}
		}
#endif
		if (!strcmp(text, "eval")) {
			struct ecmascript_string_list_item *item;

			foreach(item, interpreter->writecode) {
				if (item->string.length) {
					void *mapa = (void *)document->element_map;

					if (mapa) {
						void *el = find_in_map(mapa, item->element_offset);

						if (el) {
							el_insert_before(document, el, &item->string);
						} else {
							add_fragment(cached, 0, item->string.source, item->string.length);
							free_document(document->dom);
							document->dom = document_parse(document, &item->string);
							break;
						}
					}
				}
			}
		}

		if (document->dom) {
			struct delayed_rel *rel = (struct delayed_rel *)mem_calloc(1, sizeof(*rel));

			if (rel) {
				rel->cached = cached;
				rel->document = document;
				rel->ses = ses;
				rel->was_write = interpreter->was_write;
				object_lock(document);
				register_bottom_half(delayed_reload, rel);
			}
			interpreter->changed = 0;
			interpreter->was_write = 0;
		}
	}
}

void
ecmascript_eval(struct ecmascript_interpreter *interpreter,
                struct string *code, struct string *ret, int element_offset)
{
	ELOG
	if (!is_ecmascript_enabled_for_interpreter(interpreter)) {
		return;
	}
	interpreter->backend_nesting++;
	interpreter->element_offset = element_offset;
#ifdef CONFIG_MUJS
	mujs_eval(interpreter, code, ret);
#elif defined(CONFIG_QUICKJS)
	quickjs_eval(interpreter, code, ret);
#else
	spidermonkey_eval(interpreter, code, ret);
#endif
	interpreter->backend_nesting--;
}

#ifdef CONFIG_MUJS
static void
ecmascript_call_function(struct ecmascript_interpreter *interpreter,
                const char *fun, struct string *ret)
#elif defined(CONFIG_QUICKJS)
static void
ecmascript_call_function(struct ecmascript_interpreter *interpreter,
                JSValueConst fun, struct string *ret)
#else
static void
ecmascript_call_function(struct ecmascript_interpreter *interpreter,
                JS::HandleValue fun, struct string *ret)
#endif
{
	ELOG
	if (!is_ecmascript_enabled_for_interpreter(interpreter)) {
		return;
	}
	interpreter->backend_nesting++;
#ifdef CONFIG_MUJS
	mujs_call_function(interpreter, fun, ret);
#elif defined(CONFIG_QUICKJS)
	quickjs_call_function(interpreter, fun, ret);
#else
	spidermonkey_call_function(interpreter, fun, ret);
#endif
	interpreter->backend_nesting--;
}

#ifdef CONFIG_MUJS
static void
ecmascript_call_function_timestamp(struct ecmascript_interpreter *interpreter,
                const char *fun, struct string *ret)
#elif defined(CONFIG_QUICKJS)
static void
ecmascript_call_function_timestamp(struct ecmascript_interpreter *interpreter,
                JSValueConst fun, struct string *ret)
#else
static void
ecmascript_call_function_timestamp(struct ecmascript_interpreter *interpreter,
                JS::HandleValue fun, struct string *ret)
#endif
{
	ELOG
	if (!is_ecmascript_enabled_for_interpreter(interpreter)) {
		return;
	}
	interpreter->backend_nesting++;
#ifdef CONFIG_MUJS
	mujs_call_function_timestamp(interpreter, fun, ret);
#elif defined(CONFIG_QUICKJS)
	quickjs_call_function_timestamp(interpreter, fun, ret);
#else
	spidermonkey_call_function_timestamp(interpreter, fun, ret);
#endif
	interpreter->backend_nesting--;
}

char *
ecmascript_eval_stringback(struct ecmascript_interpreter *interpreter,
			   struct string *code)
{
	ELOG
	char *result;

	if (!is_ecmascript_enabled_for_interpreter(interpreter)) {
		return NULL;
	}
	interpreter->backend_nesting++;
#ifdef CONFIG_MUJS
	result = mujs_eval_stringback(interpreter, code);
#elif defined(CONFIG_QUICKJS)
	result = quickjs_eval_stringback(interpreter, code);
#else
	result = spidermonkey_eval_stringback(interpreter, code);
#endif
	interpreter->backend_nesting--;

	check_for_rerender(interpreter, "stringback");

	return result;
}

void
ecmascript_timeout_dialog(struct terminal *term, int max_exec_time)
{
	ELOG
	info_box(term, MSGBOX_FREE_TEXT,
		 N_("JavaScript Emergency"), ALIGN_LEFT,
		 msg_text(term,
		  N_("A script embedded in the current document was running\n"
		  "for more than %d seconds. This probably means there is\n"
		  "a bug in the script and it could have halted the whole\n"
		  "ELinks, so the script execution was interrupted."),
		  max_exec_time));

}

void
ecmascript_set_action(char **action, char *string)
{
	ELOG
	struct uri *protocol;

	trim_chars(string, ' ', NULL);
	protocol = get_uri(string, URI_PROTOCOL);

	if (protocol) { /* full uri with protocol */
		done_uri(protocol);
		mem_free_set(action, string);
	} else {
		if (dir_sep(string[0])) { /* absolute uri, TODO: disk drive under WIN32 */
			struct uri *uri = get_uri(*action, URI_HTTP_REFERRER_HOST);

			if (uri->protocol == PROTOCOL_FILE) {
				mem_free_set(action, straconcat(struri(uri), string, (char *) NULL));
			}
			else
				mem_free_set(action, straconcat(struri(uri), string + 1, (char *) NULL));
			done_uri(uri);
			mem_free(string);
		} else { /* relative uri */
			char *last_slash = strrchr(*action, '/');
			char *new_action;

			if (last_slash) *(last_slash + 1) = '\0';
			new_action = straconcat(*action, string,
						(char *) NULL);
			mem_free_set(action, new_action);
			mem_free(string);
		}
	}
}

/* Timer callback for @interpreter->vs->doc_view->document->timeout.
 * As explained in @install_timer, this function must erase the
 * expired timer ID from all variables.  */
static void
ecmascript_timeout_handler(void *val)
{
	ELOG
	struct ecmascript_timeout *t = (struct ecmascript_timeout *)val;
	struct ecmascript_interpreter *interpreter = t->interpreter;

	if (t->timeout_next < 0) {
		goto skip;
	}

	if (!interpreter->vs->doc_view) {
		goto skip;
	}

//	assertm(interpreter->vs->doc_view != NULL,
//		"setTimeout: vs with no document (e_f %d)",
//		interpreter->vs->ecmascript_fragile);
	ecmascript_eval(interpreter, &t->code, NULL, 0);

	if (t->timeout_next > 0) {
		install_timer(&t->tid, t->timeout_next, ecmascript_timeout_handler, t);
		add_to_map_timer(t);
	} else {
skip:
	/* The expired timer ID has now been erased.  */
		t->tid = TIMER_ID_UNDEF;
		del_from_list(t);
		done_string(&t->code);
		mem_free(t);
	}

	check_for_rerender(interpreter, "handler");
}

static void
ecmascript_request(void *val)
{
	ELOG
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)val;

	interpreter->timestamp += (double)REQUEST_ANIMATION_FRAME;
	install_timer(&interpreter->ani, REQUEST_ANIMATION_FRAME, ecmascript_request, val);

	if (interpreter->vs->doc_view != NULL) {
#ifdef CONFIG_ECMASCRIPT_SMJS
		if (interpreter->request_func) {
			JS::RootedValue vfun((JSContext *)interpreter->backend_data, *(interpreter->request_func));
			delete interpreter->request_func;
			interpreter->request_func = NULL;
			ecmascript_call_function_timestamp(interpreter, vfun, NULL);
			check_for_rerender(interpreter, "request");
		}
#endif
#ifdef CONFIG_QUICKJS
		if (!JS_IsNull(interpreter->request_func)) {
			JSValue tmp = JS_DupValue(interpreter->backend_data, interpreter->request_func);
			JS_FreeValue(interpreter->backend_data, interpreter->request_func);
			interpreter->request_func = JS_NULL;
			ecmascript_call_function_timestamp(interpreter, tmp, NULL);
			JS_FreeValue(interpreter->backend_data, tmp);
			check_for_rerender(interpreter, "request");
		}
#endif
#ifdef CONFIG_MUJS
		if (interpreter->request_func) {
			const char *handle = interpreter->request_func;
			interpreter->request_func = NULL;
			ecmascript_call_function_timestamp(interpreter, handle, NULL);
			check_for_rerender(interpreter, "request");
		}
#endif
	}
}


#if defined(CONFIG_ECMASCRIPT_SMJS) || defined(CONFIG_QUICKJS) || defined(CONFIG_MUJS)
/* Timer callback for @interpreter->vs->doc_view->document->timeout.
 * As explained in @install_timer, this function must erase the
 * expired timer ID from all variables.  */
static void
ecmascript_timeout_handler2(void *val)
{
	ELOG
	struct ecmascript_timeout *t = (struct ecmascript_timeout *)val;
	struct ecmascript_interpreter *interpreter = t->interpreter;

	if (t->timeout_next < 0) {
		goto skip;
	}
#if 0
	assertm(interpreter->vs->doc_view != NULL,
		"setTimeout: vs with no document (e_f %d)",
		interpreter->vs->ecmascript_fragile);
#endif

	if (interpreter->vs->doc_view) {
#ifdef CONFIG_ECMASCRIPT_SMJS
		JS::RootedValue vfun((JSContext *)interpreter->backend_data, *(t->fun));
		ecmascript_call_function(interpreter, vfun, NULL);
#else
		ecmascript_call_function(interpreter, t->fun, NULL);
#endif
	} else {
		t->timeout_next = 0;
	}
	if (t->timeout_next > 0) {
		install_timer(&t->tid, t->timeout_next, ecmascript_timeout_handler2, t);
		add_to_map_timer(t);
	} else {
skip:
		t->tid = TIMER_ID_UNDEF;
		/* The expired timer ID has now been erased.  */
		del_from_list(t);
		done_string(&t->code);
#ifdef CONFIG_QUICKJS
		JS_FreeValue(t->ctx, t->fun);
#endif
#ifdef CONFIG_ECMASCRIPT_SMJS
		delete(t->fun);
#endif
#ifdef CONFIG_MUJS
//	js_unref(t->ctx, t->fun);
#endif
		mem_free(t);
	}

	check_for_rerender(interpreter, "handler2");
}
#endif

struct ecmascript_timeout *
ecmascript_set_timeout(void *c, char *code, int timeout, int timeout_next)
{
	ELOG
#ifdef CONFIG_QUICKJS
	JSContext *ctx = (JSContext *)c;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
#endif
#ifdef CONFIG_ECMASCRIPT_SMJS
	JSContext *ctx = (JSContext *)c;
	JS::Realm *comp = js::GetContextRealm((JSContext *)ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
#endif
#ifdef CONFIG_MUJS
	js_State *ctx = (js_State *)c;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(ctx);
#endif
	assert(interpreter && interpreter->vs->doc_view->document);
	if (!code) return NULL;
	struct ecmascript_timeout *t = (struct ecmascript_timeout *)mem_calloc(1, sizeof(*t));

	if (!t) {
		mem_free(code);
		return NULL;
	}
	if (!init_string(&t->code)) {
		mem_free(t);
		mem_free(code);
		return NULL;
	}
	add_to_string(&t->code, code);
	mem_free(code);

	t->interpreter = interpreter;
	t->ctx = ctx;
	t->timeout_next = timeout_next;
#ifdef CONFIG_QUICKJS
	t->fun = JS_NULL;
#endif
	add_to_list(interpreter->timeouts, t);
	install_timer(&t->tid, timeout, ecmascript_timeout_handler, t);
	add_to_map_timer(t);

	return t;
}

#ifdef CONFIG_ECMASCRIPT_SMJS
int
ecmascript_set_request2(void *c, JS::HandleValue f)
{
	ELOG
	JSContext *ctx = (JSContext *)c;
	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	assert(interpreter && interpreter->vs->doc_view->document);

	JS::RootedValue fun((JSContext *)interpreter->backend_data, f);

	if (interpreter->request_func) {
		delete interpreter->request_func;
	}
	interpreter->request_func = new JS::Heap<JS::Value>(fun);
	interpreter->request++;

	return interpreter->request;
}

struct ecmascript_timeout *
ecmascript_set_timeout2(void *c, JS::HandleValue f, int timeout, int timeout_next)
{
	ELOG
	JSContext *ctx = (JSContext *)c;
	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	assert(interpreter && interpreter->vs->doc_view->document);

	struct ecmascript_timeout *t = (struct ecmascript_timeout *)mem_calloc(1, sizeof(*t));

	if (!t) {
		return NULL;
	}
	if (!init_string(&t->code)) {
		mem_free(t);
		return NULL;
	}
	t->interpreter = interpreter;
	t->ctx = ctx;
	t->timeout_next = timeout_next;
	JS::RootedValue fun((JSContext *)interpreter->backend_data, f);
	t->fun = new JS::Heap<JS::Value>(fun);
	add_to_list(interpreter->timeouts, t);
	install_timer(&t->tid, timeout, ecmascript_timeout_handler2, t);
	add_to_map_timer(t);

	return t;
}
#endif

#ifdef CONFIG_QUICKJS
int
ecmascript_set_request2(void *c, JSValueConst fun)
{
	ELOG
	JSContext *ctx = (JSContext *)c;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	assert(interpreter && interpreter->vs->doc_view->document);

	JS_FreeValue(ctx, interpreter->request_func);
	interpreter->request_func = JS_DupValue(ctx, fun);
	interpreter->request++;

	return interpreter->request;
}


struct ecmascript_timeout *
ecmascript_set_timeout2q(void *c, JSValueConst fun, int timeout, int timeout_next)
{
	ELOG
	JSContext *ctx = (JSContext *)c;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	assert(interpreter && interpreter->vs->doc_view->document);
	struct ecmascript_timeout *t = (struct ecmascript_timeout *)mem_calloc(1, sizeof(*t));

	if (!t) {
		return NULL;
	}
	if (!init_string(&t->code)) {
		mem_free(t);
		return NULL;
	}
	t->interpreter = interpreter;
	t->ctx = ctx;
	t->timeout_next = timeout_next;
	t->fun = JS_DupValue(ctx, fun);
	add_to_list(interpreter->timeouts, t);
	install_timer(&t->tid, timeout, ecmascript_timeout_handler2, t);
	add_to_map_timer(t);

	return t;
}
#endif

#ifdef CONFIG_MUJS
int
ecmascript_set_request2(js_State *J, const char *handle)
{
	ELOG
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	assert(interpreter && interpreter->vs->doc_view->document);

	interpreter->request_func = handle;
	interpreter->request++;

	return interpreter->request;
}

struct ecmascript_timeout *
ecmascript_set_timeout2m(js_State *J, const char *handle, int timeout, int timeout_next)
{
	ELOG
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	assert(interpreter && interpreter->vs->doc_view->document);

	struct ecmascript_timeout *t = (struct ecmascript_timeout *)mem_calloc(1, sizeof(*t));

	if (!t) {
		return NULL;
	}
	if (!init_string(&t->code)) {
		mem_free(t);
		return NULL;
	}
	t->interpreter = interpreter;
	t->ctx = J;
	t->fun = handle;
	t->timeout_next = timeout_next;

	add_to_list(interpreter->timeouts, t);
	install_timer(&t->tid, timeout, ecmascript_timeout_handler2, t);
	add_to_map_timer(t);

	return t;
}
#endif

int
ecmascript_found(struct ecmascript_interpreter *interpreter)
{
	ELOG
	struct ecmascript_interpreter *i;

	foreach (i, ecmascript_interpreters) {
		if (i == interpreter) {
			return 1;
		}
	}

	return 0;
}

static void
init_ecmascript_module(struct module *module)
{
	ELOG
	char *xdg_config_home = get_xdg_config_home();
	read_url_list();

	if (xdg_config_home) {
		/* ecmascript console log */
		console_log_filename = straconcat(xdg_config_home, "console.log", NULL);
		console_warn_filename = straconcat(xdg_config_home, "console.warn", NULL);
		console_error_filename = program.testjs ? stracpy("/dev/stderr") : straconcat(xdg_config_home, "console.err", NULL);
		/* ecmascript local storage db location */
#ifdef CONFIG_OS_DOS
		local_storage_filename = stracpy("elinks_ls.db");
#else
		local_storage_filename = straconcat(xdg_config_home, "elinks_ls.db", NULL);
#endif
	}
	ecmascript_enabled = get_opt_bool("ecmascript.enable", NULL);
#ifdef CONFIG_LIBCURL
#ifdef CONFIG_MEMCOUNT
	curl_global_init_mem(CURL_GLOBAL_DEFAULT, el_curl_malloc, el_curl_free, el_curl_realloc, el_curl_strdup, el_curl_calloc);
#else
	curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
#endif
	init_map_timer();
}

static void
done_ecmascript_module(struct module *module)
{
	ELOG
#ifdef CONFIG_LIBCURL
	curl_global_cleanup();
#endif
	free_string_list(&allowed_urls);
	free_string_list(&disallowed_urls);
	mem_free_if(console_log_filename);
	mem_free_if(console_error_filename);
	mem_free_if(console_warn_filename);
	mem_free_if(local_storage_filename);
	done_map_timer();
}

static struct module *ecmascript_modules[] = {
#ifdef CONFIG_ECMASCRIPT_SMJS
	&spidermonkey_module,
#endif
#ifdef CONFIG_MUJS
	&mujs_module,
#endif
#ifdef CONFIG_QUICKJS
	&quickjs_module,
#endif
	NULL,
};

static void
delayed_goto(void *data)
{
	ELOG
	struct delayed_goto *deg = (struct delayed_goto *)data;

	assert(deg);

	if (deg->vs->doc_view) {
		goto_uri_frame(deg->vs->doc_view->session, deg->uri,
		               deg->vs->doc_view->name,
		               deg->reload ? CACHE_MODE_FORCE_RELOAD : CACHE_MODE_NORMAL);
	}
	done_uri(deg->uri);
	mem_free(deg);
}

void
location_goto_const(struct document_view *doc_view, const char *url, int reload)
{
	ELOG
	char *url2 = stracpy(url);

	if (url2) {
		location_goto(doc_view, url2, reload);
		mem_free(url2);
	}
}

void
location_goto(struct document_view *doc_view, char *url, int reload)
{
	ELOG
	char *new_abs_url;
	struct uri *new_uri;
	struct delayed_goto *deg;

	/* Workaround for bug 611. Does not crash, but may lead to infinite loop.*/
	if (!doc_view) return;
	new_abs_url = join_urls(doc_view->document->uri,
	                        trim_chars(url, ' ', 0));
	if (!new_abs_url)
		return;
	new_uri = get_uri(new_abs_url, URI_NONE);
	mem_free(new_abs_url);
	if (!new_uri)
		return;
	deg = (struct delayed_goto *)mem_calloc(1, sizeof(*deg));
	if (!deg) {
		done_uri(new_uri);
		return;
	}
	assert(doc_view->vs);
	deg->vs = doc_view->vs;
	deg->uri = new_uri;
	deg->reload = reload;
	/* It does not seem to be very safe inside of frames to
	 * call goto_uri() right away. */
	register_bottom_half(delayed_goto, deg);
}

struct module ecmascript_module = struct_module(
	/* name: */		N_("ECMAScript"),
	/* options: */		ecmascript_options,
	/* events: */		NULL,
	/* submodules: */	ecmascript_modules,
	/* data: */		NULL,
	/* init: */		init_ecmascript_module,
	/* done: */		done_ecmascript_module,
	/* getname: */	NULL
);
