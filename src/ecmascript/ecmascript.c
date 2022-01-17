/* Base ECMAScript file. Mostly a proxy for specific library backends. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>

#include "elinks.h"

#include "config/home.h"
#include "config/options.h"
#include "dialogs/status.h"
#include "document/document.h"
#include "document/renderer.h"
#include "document/view.h"
#include "document/xml/renderer.h"
#include "document/xml/renderer2.h"
#include "ecmascript/ecmascript.h"
#ifdef CONFIG_QUICKJS
#include "ecmascript/quickjs.h"
#else
#include "ecmascript/spidermonkey.h"
#endif
#include "ecmascript/timer.h"
#include "intl/libintl.h"
#include "main/module.h"
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
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/view.h" /* current_frame() */
#include "viewer/text/form.h" /* <-ecmascript_reset_state() */
#include "viewer/text/vs.h"

#include <libxml/tree.h>
#include <libxml/HTMLparser.h>
#include <libxml++/libxml++.h>

#include <map>

std::map<struct timer *, bool> map_timer;

/* TODO: We should have some kind of ACL for the scripts - i.e. ability to
 * disallow the scripts to open new windows (or so that the windows are always
 * directed to tabs, this particular option would be a tristate), disallow
 * messing with your menubar/statusbar visibility, disallow changing the
 * statusbar content etc. --pasky */

static union option_info ecmascript_options[] = {
	INIT_OPT_TREE("", N_("ECMAScript"),
		"ecmascript", OPT_ZERO,
		N_("ECMAScript options.")),

	INIT_OPT_BOOL("ecmascript", N_("Enable"),
		"enable", OPT_ZERO, 0,
		N_("Whether to run those scripts inside of documents.")),

	INIT_OPT_BOOL("ecmascript", N_("Console log"),
		"enable_console_log", OPT_ZERO, 0,
		N_("When enabled logs will be appended to ~/.elinks/console.log.")),

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

	INIT_OPT_BOOL("ecmascript", N_("Pop-up window blocking"),
		"block_window_opening", OPT_ZERO, 0,
		N_("Whether to disallow scripts to open new windows or tabs.")),

	NULL_OPTION_INFO,
};

static int interpreter_count;

static INIT_LIST_OF(struct string_list_item, allowed_urls);
static INIT_LIST_OF(struct string_list_item, disallowed_urls);

char *console_error_filename;
char *console_log_filename;

char *local_storage_filename;

int local_storage_ready;

static int
is_prefix(char *prefix, char *url, int dl)
{
	return memcmp(prefix, url, dl);
}

static void
read_url_list(void)
{
	char line[4096];
	char *filename;
	FILE *f;

	if (!elinks_home) {
		return;
	}

	filename = straconcat(elinks_home, STRING_DIR_SEP, ALLOWED_ECMASCRIPT_URL_PREFIXES, NULL);

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

	filename = straconcat(elinks_home, STRING_DIR_SEP, DISALLOWED_ECMASCRIPT_URL_PREFIXES, NULL);

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

static int ecmascript_enabled;

void
toggle_ecmascript(struct session *ses)
{
	ecmascript_enabled = !ecmascript_enabled;

	if (ecmascript_enabled) {
		mem_free_set(&ses->status.window_status, stracpy(_("Ecmascript enabled", ses->tab->term)));
	} else {
		mem_free_set(&ses->status.window_status, stracpy(_("Ecmascript disabled", ses->tab->term)));
	}
	print_screen_status(ses);
}

int
get_ecmascript_enable(struct ecmascript_interpreter *interpreter)
{
	struct string_list_item *item;
	char *url;

	if (!ecmascript_enabled || !get_opt_bool("ecmascript.enable", NULL)
	|| !interpreter || !interpreter->vs || !interpreter->vs->doc_view
	|| !interpreter->vs->doc_view->document || !interpreter->vs->doc_view->document->uri) {
		return 0;
	}

	if (list_empty(allowed_urls) && list_empty(disallowed_urls)) {
		return 1;
	}

	url = get_uri_string(interpreter->vs->doc_view->document->uri, URI_PUBLIC);
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
#ifdef CONFIG_QUICKJS
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
	return interpreter;
}

void
ecmascript_put_interpreter(struct ecmascript_interpreter *interpreter)
{
	assert(interpreter);
	assert(interpreter->backend_nesting == 0);
	/* If the assertion fails, it is better to leak the
	 * interpreter than to corrupt memory.  */
	if_assert_failed return;
#ifdef CONFIG_QUICKJS
	quickjs_put_interpreter(interpreter);
#else
	spidermonkey_put_interpreter(interpreter);
#endif
	free_string_list(&interpreter->onload_snippets);
	done_string(&interpreter->code);
	/* Is it superfluous? */
	if (interpreter->vs->doc_view)
		kill_timer(&interpreter->vs->doc_view->document->timeout);
	interpreter->vs->ecmascript = NULL;
	interpreter->vs->ecmascript_fragile = 1;
	mem_free(interpreter);
	--interpreter_count;
}

int
ecmascript_get_interpreter_count(void)
{
	return interpreter_count;
}

static void
delayed_reload(void *data)
{
	struct delayed_rel *rel = data;

	assert(rel);
	reset_document(rel->document);
	render_xhtml_document(rel->cached, rel->document, NULL);
	sort_links(rel->document);
	draw_formatted(rel->ses, 0);
	mem_free(rel);
}

void
check_for_rerender(struct ecmascript_interpreter *interpreter, const char* text)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %s %d\n", __FILE__, __FUNCTION__, text, interpreter->changed);
#endif
	if (interpreter->changed) {
		struct document_view *doc_view = interpreter->vs->doc_view;
		struct document *document = doc_view->document;
		struct session *ses = doc_view->session;
		struct cache_entry *cached = document->cached;
		struct fragment *f = get_cache_fragment(cached);

		//fprintf(stderr, "%s\n", text);

		if (document->dom) {
			interpreter->changed = false;

			struct delayed_rel *rel = (struct delayed_rel *)mem_calloc(1, sizeof(*rel));

			if (rel) {
				rel->cached = cached;
				rel->document = document;
				rel->ses = ses;
				object_lock(document);
				register_bottom_half(delayed_reload, rel);
			}
		}
	}
}

void
ecmascript_eval(struct ecmascript_interpreter *interpreter,
                struct string *code, struct string *ret)
{
	if (!get_ecmascript_enable(interpreter))
		return;
	assert(interpreter);
	interpreter->backend_nesting++;
#ifdef CONFIG_QUICKJS
	quickjs_eval(interpreter, code, ret);
#else
	spidermonkey_eval(interpreter, code, ret);
#endif
	interpreter->backend_nesting--;
}

#ifdef CONFIG_QUICKJS
static void
ecmascript_call_function(struct ecmascript_interpreter *interpreter,
                JSValueConst fun, struct string *ret)
#else
static void
ecmascript_call_function(struct ecmascript_interpreter *interpreter,
                JS::HandleValue fun, struct string *ret)
#endif
{
	if (!get_ecmascript_enable(interpreter))
		return;
	assert(interpreter);
	interpreter->backend_nesting++;
#ifdef CONFIG_QUICKJS
	quickjs_call_function(interpreter, fun, ret);
#else
	spidermonkey_call_function(interpreter, fun, ret);
#endif
	interpreter->backend_nesting--;
}

char *
ecmascript_eval_stringback(struct ecmascript_interpreter *interpreter,
			   struct string *code)
{
	char *result;

	if (!get_ecmascript_enable(interpreter))
		return NULL;
	assert(interpreter);
	interpreter->backend_nesting++;
#ifdef CONFIG_QUICKJS
	result = quickjs_eval_stringback(interpreter, code);
#else
	result = spidermonkey_eval_stringback(interpreter, code);
#endif
	interpreter->backend_nesting--;

	check_for_rerender(interpreter, "stringback");

	return result;
}

int
ecmascript_eval_boolback(struct ecmascript_interpreter *interpreter,
			 struct string *code)
{
	int result;

	if (!get_ecmascript_enable(interpreter))
		return -1;
	assert(interpreter);
	interpreter->backend_nesting++;
#ifdef CONFIG_QUICKJS
	result = quickjs_eval_boolback(interpreter, code);
#else
	result = spidermonkey_eval_boolback(interpreter, code);
#endif
	interpreter->backend_nesting--;

	check_for_rerender(interpreter, "boolback");

	return result;
}

void
ecmascript_detach_form_view(struct form_view *fv)
{
#ifdef CONFIG_QUICKJS
	quickjs_detach_form_view(fv);
#else
	spidermonkey_detach_form_view(fv);
#endif
}

void ecmascript_detach_form_state(struct form_state *fs)
{
#ifdef CONFIG_QUICKJS
	quickjs_detach_form_state(fs);
#else
	spidermonkey_detach_form_state(fs);
#endif
}

void ecmascript_moved_form_state(struct form_state *fs)
{
#ifdef CONFIG_QUICKJS
	quickjs_moved_form_state(fs);
#else
	spidermonkey_moved_form_state(fs);
#endif
}

void
ecmascript_reset_state(struct view_state *vs)
{
	struct form_view *fv;
	int i;

	/* Normally, if vs->ecmascript == NULL, the associated
	 * ecmascript_obj pointers are also NULL.  However, they might
	 * be non-NULL if the ECMAScript objects have been lazily
	 * created because of scripts running in sibling HTML frames.  */
	foreach (fv, vs->forms)
		ecmascript_detach_form_view(fv);
	for (i = 0; i < vs->form_info_len; i++)
		ecmascript_detach_form_state(&vs->form_info[i]);

	vs->ecmascript_fragile = 0;
	if (vs->ecmascript)
		ecmascript_put_interpreter(vs->ecmascript);

	vs->ecmascript = ecmascript_get_interpreter(vs);
	if (!vs->ecmascript)
		vs->ecmascript_fragile = 1;
}

void
ecmascript_protocol_handler(struct session *ses, struct uri *uri)
{
	struct document_view *doc_view = current_frame(ses);
	struct string current_url = INIT_STRING(struri(uri), (int)strlen(struri(uri)));
	char *redirect_url, *redirect_abs_url;
	struct uri *redirect_uri;

	if (!doc_view) /* Blank initial document. TODO: Start at about:blank? */
		return;
	assert(doc_view->vs);
	if (doc_view->vs->ecmascript_fragile)
		ecmascript_reset_state(doc_view->vs);
	if (!doc_view->vs->ecmascript)
		return;

	redirect_url = ecmascript_eval_stringback(doc_view->vs->ecmascript,
		&current_url);
	if (!redirect_url)
		return;
	/* XXX: This code snippet is duplicated over here,
	 * location_set_property(), html_a() and who knows where else. */
	redirect_abs_url = join_urls(doc_view->document->uri,
	                             trim_chars(redirect_url, ' ', 0));
	mem_free(redirect_url);
	if (!redirect_abs_url)
		return;
	redirect_uri = get_uri(redirect_abs_url, URI_NONE);
	mem_free(redirect_abs_url);
	if (!redirect_uri)
		return;

	/* XXX: Is that safe to do at this point? --pasky */
	goto_uri_frame(ses, redirect_uri, doc_view->name,
		CACHE_MODE_NORMAL);
	done_uri(redirect_uri);
}


void
ecmascript_timeout_dialog(struct terminal *term, int max_exec_time)
{
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
			char *last_slash = strrchr((const char *)*action, '/');
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
ecmascript_timeout_handler(void *i)
{
	struct ecmascript_interpreter *interpreter = i;

	assertm(interpreter->vs->doc_view != NULL,
		"setTimeout: vs with no document (e_f %d)",
		interpreter->vs->ecmascript_fragile);
	interpreter->vs->doc_view->document->timeout = TIMER_ID_UNDEF;
	/* The expired timer ID has now been erased.  */

	ecmascript_eval(interpreter, &interpreter->code, NULL);
	check_for_rerender(interpreter, "handler");
}

#if defined(CONFIG_ECMASCRIPT_SMJS) || defined(CONFIG_QUICKJS)
/* Timer callback for @interpreter->vs->doc_view->document->timeout.
 * As explained in @install_timer, this function must erase the
 * expired timer ID from all variables.  */
static void
ecmascript_timeout_handler2(void *i)
{
	struct ecmascript_interpreter *interpreter = i;

	assertm(interpreter->vs->doc_view != NULL,
		"setTimeout: vs with no document (e_f %d)",
		interpreter->vs->ecmascript_fragile);
	interpreter->vs->doc_view->document->timeout = TIMER_ID_UNDEF;
	/* The expired timer ID has now been erased.  */

	ecmascript_call_function(interpreter, interpreter->fun, NULL);
	check_for_rerender(interpreter, "handler2");
}
#endif

timer_id_T
ecmascript_set_timeout(struct ecmascript_interpreter *interpreter, char *code, int timeout)
{
	assert(interpreter && interpreter->vs->doc_view->document);
	if (!code) return nullptr;
	if (interpreter->code.source) {
		done_string(&interpreter->code);
	}
	if (!init_string(&interpreter->code)) {
		mem_free(code);
		return nullptr;
	}
	add_to_string(&interpreter->code, code);
	mem_free(code);
	if (found_in_map_timer(interpreter->vs->doc_view->document->timeout)) {
		kill_timer(&interpreter->vs->doc_view->document->timeout);
	}
	install_timer(&interpreter->vs->doc_view->document->timeout, timeout, ecmascript_timeout_handler, interpreter);

	return interpreter->vs->doc_view->document->timeout;
}

#ifdef CONFIG_ECMASCRIPT_SMJS
timer_id_T
ecmascript_set_timeout2(struct ecmascript_interpreter *interpreter, JS::HandleValue f, int timeout)
{
	assert(interpreter && interpreter->vs->doc_view->document);
	if (interpreter->code.source) {
		done_string(&interpreter->code);
	}

	if (!init_string(&interpreter->code)) {
		return TIMER_ID_UNDEF;
	}
	if (found_in_map_timer(interpreter->vs->doc_view->document->timeout)) {
		kill_timer(&interpreter->vs->doc_view->document->timeout);
	}
	JS::RootedValue fun((JSContext *)interpreter->backend_data, f);
	interpreter->fun = fun;
	install_timer(&interpreter->vs->doc_view->document->timeout, timeout, ecmascript_timeout_handler2, interpreter);

	return interpreter->vs->doc_view->document->timeout;
}
#endif

#ifdef CONFIG_QUICKJS
timer_id_T
ecmascript_set_timeout2q(struct ecmascript_interpreter *interpreter, JSValueConst fun, int timeout)
{
	assert(interpreter && interpreter->vs->doc_view->document);
	if (interpreter->code.source) {
		done_string(&interpreter->code);
	}
	if (!init_string(&interpreter->code)) {
		return TIMER_ID_UNDEF;
	}
	if (found_in_map_timer(interpreter->vs->doc_view->document->timeout)) {
		kill_timer(&interpreter->vs->doc_view->document->timeout);
	}
	interpreter->fun = fun;
	install_timer(&interpreter->vs->doc_view->document->timeout, timeout, ecmascript_timeout_handler2, interpreter);

	return interpreter->vs->doc_view->document->timeout;
}
#endif

static void
init_ecmascript_module(struct module *module)
{
	read_url_list();

	if (elinks_home) {
		/* ecmascript console log */
		console_log_filename = straconcat(elinks_home, "/console.log", NULL);
		console_error_filename = straconcat(elinks_home, "/console.err", NULL);
		/* ecmascript local storage db location */
		local_storage_filename = straconcat(elinks_home, "/elinks_ls.db", NULL);
	}
	ecmascript_enabled = get_opt_bool("ecmascript.enable", NULL);
}

static void
done_ecmascript_module(struct module *module)
{
	free_string_list(&allowed_urls);
	mem_free_if(console_log_filename);
	mem_free_if(local_storage_filename);
}

static struct module *ecmascript_modules[] = {
#ifdef CONFIG_ECMASCRIPT_SMJS
	&spidermonkey_module,
#endif
#ifdef CONFIG_QUICKJS
	&quickjs_module,
#endif
	NULL,
};

void
free_document(void *doc)
{
	if (!doc) {
		return;
	}
	xmlpp::Document *docu = doc;
	delete docu;
}

void *
document_parse(struct document *document)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct cache_entry *cached = document->cached;
	struct fragment *f = get_cache_fragment(cached);

	if (!f || !f->length) {
		return NULL;
	}

	struct string str;
	if (!init_string(&str)) {
		return NULL;
	}

	add_bytes_to_string(&str, f->data, f->length);

	// Parse HTML and create a DOM tree
	xmlDoc* doc = htmlReadDoc((xmlChar*)str.source, NULL, get_cp_mime_name(document->cp),
	HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
	// Encapsulate raw libxml document in a libxml++ wrapper
	xmlpp::Document *docu = new xmlpp::Document(doc);
	done_string(&str);

	return (void *)docu;
}

static void
delayed_goto(void *data)
{
	struct delayed_goto *deg = data;

	assert(deg);
	if (deg->vs->doc_view
	    && deg->vs->doc_view == deg->vs->doc_view->session->doc_view) {
		goto_uri_frame(deg->vs->doc_view->session, deg->uri,
		               deg->vs->doc_view->name,
			       CACHE_MODE_NORMAL);
	}
	done_uri(deg->uri);
	mem_free(deg);
}

void
location_goto_const(struct document_view *doc_view, const char *url)
{
	char *url2 = stracpy(url);

	if (url2) {
		location_goto(doc_view, url2);
		mem_free(url2);
	}
}

void
location_goto(struct document_view *doc_view, char *url)
{
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
	/* done: */		done_ecmascript_module
);
