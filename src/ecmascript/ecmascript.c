/* Base ECMAScript file. Mostly a proxy for specific library backends. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "config/options.h"
#include "document/document.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/see.h"
#include "ecmascript/spidermonkey.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
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
#include "viewer/text/view.h" /* current_frame() */
#include "viewer/text/form.h" /* <-ecmascript_reset_state() */
#include "viewer/text/vs.h"


/* TODO: We should have some kind of ACL for the scripts - i.e. ability to
 * disallow the scripts to open new windows (or so that the windows are always
 * directed to tabs, this particular option would be a tristate), disallow
 * messing with your menubar/statusbar visibility, disallow changing the
 * statusbar content etc. --pasky */

static struct option_info ecmascript_options[] = {
	INIT_OPT_TREE("", N_("ECMAScript"),
		"ecmascript", 0,
		N_("ECMAScript options.")),

	INIT_OPT_BOOL("ecmascript", N_("Enable"),
		"enable", 0, 0,
		N_("Whether to run those scripts inside of documents.")),

	INIT_OPT_BOOL("ecmascript", N_("Script error reporting"),
		"error_reporting", 0, 0,
		N_("Open a message box when a script reports an error.")),

	INIT_OPT_BOOL("ecmascript", N_("Ignore <noscript> content"),
		"ignore_noscript", 0, 0,
		N_("Whether to ignore content enclosed by the <noscript> tag "
		"when ECMAScript is enabled.")),

	INIT_OPT_INT("ecmascript", N_("Maximum execution time"),
		"max_exec_time", 0, 1, 3600, 5,
		N_("Maximum execution time in seconds for a script.")),

	INIT_OPT_BOOL("ecmascript", N_("Pop-up window blocking"),
		"block_window_opening", 0, 0,
		N_("Whether to disallow scripts to open new windows or tabs.")),

	NULL_OPTION_INFO,
};

struct ecmascript_interpreter *
ecmascript_get_interpreter(struct view_state *vs)
{
	struct ecmascript_interpreter *interpreter;

	assert(vs);

	interpreter = mem_calloc(1, sizeof(*interpreter));
	if (!interpreter)
		return NULL;

	interpreter->vs = vs;
	interpreter->vs->ecmascript_fragile = 0;
	init_list(interpreter->onload_snippets);
	/* The following backend call reads interpreter->vs.  */
	if (
#ifdef CONFIG_ECMASCRIPT_SEE
	    !see_get_interpreter(interpreter)
#else
	    !spidermonkey_get_interpreter(interpreter)
#endif
	    ) {
		/* Undo what was done above.  */
		interpreter->vs->ecmascript_fragile = 1;
		mem_free(interpreter);
		return NULL;
	}
		
	init_string(&interpreter->code);
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

#ifdef CONFIG_ECMASCRIPT_SEE
	see_put_interpreter(interpreter);
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
}

void
ecmascript_eval(struct ecmascript_interpreter *interpreter,
                struct string *code, struct string *ret)
{
	if (!get_ecmascript_enable())
		return;
	assert(interpreter);
	interpreter->backend_nesting++;
#ifdef CONFIG_ECMASCRIPT_SEE
	see_eval(interpreter, code, ret);
#else
	spidermonkey_eval(interpreter, code, ret);
#endif
	interpreter->backend_nesting--;
}

unsigned char *
ecmascript_eval_stringback(struct ecmascript_interpreter *interpreter,
			   struct string *code)
{
	unsigned char *result;

	if (!get_ecmascript_enable())
		return NULL;
	assert(interpreter);
	interpreter->backend_nesting++;
#ifdef CONFIG_ECMASCRIPT_SEE
	result = see_eval_stringback(interpreter, code);
#else
	result = spidermonkey_eval_stringback(interpreter, code);
#endif
	interpreter->backend_nesting--;
	return result;
}

int
ecmascript_eval_boolback(struct ecmascript_interpreter *interpreter,
			 struct string *code)
{
	int result;

	if (!get_ecmascript_enable())
		return -1;
	assert(interpreter);
	interpreter->backend_nesting++;
#ifdef CONFIG_ECMASCRIPT_SEE
	result = see_eval_boolback(interpreter, code);
#else
	result = spidermonkey_eval_boolback(interpreter, code);
#endif
	interpreter->backend_nesting--;
	return result;
}

void
ecmascript_detach_form_view(struct form_view *fv)
{
#ifdef CONFIG_ECMASCRIPT_SEE
	see_detach_form_view(fv);
#else
	spidermonkey_detach_form_view(fv);
#endif
}

void ecmascript_detach_form_state(struct form_state *fs)
{
#ifdef CONFIG_ECMASCRIPT_SEE
	see_detach_form_state(fs);
#else
	spidermonkey_detach_form_state(fs);
#endif
}

void ecmascript_moved_form_state(struct form_state *fs)
{
#ifdef CONFIG_ECMASCRIPT_SEE
	see_moved_form_state(fs);
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
	struct string current_url = INIT_STRING(struri(uri), strlen(struri(uri)));
	unsigned char *redirect_url, *redirect_abs_url;
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
	redirect_uri = get_uri(redirect_abs_url, 0);
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
ecmascript_set_action(unsigned char **action, unsigned char *string)
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
				mem_free_set(action, straconcat(struri(uri), string, (unsigned char *) NULL));
			}
			else
				mem_free_set(action, straconcat(struri(uri), string + 1, (unsigned char *) NULL));
			done_uri(uri);
			mem_free(string);
		} else { /* relative uri */
			unsigned char *last_slash = strrchr(*action, '/');
			unsigned char *new_action;

			if (last_slash) *(last_slash + 1) = '\0';
			new_action = straconcat(*action, string,
						(unsigned char *) NULL);
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
}

void
ecmascript_set_timeout(struct ecmascript_interpreter *interpreter, unsigned char *code, int timeout)
{
	assert(interpreter && interpreter->vs->doc_view->document);
	if (!code) return;
	done_string(&interpreter->code);
	init_string(&interpreter->code);
	add_to_string(&interpreter->code, code);
	mem_free(code);
	kill_timer(&interpreter->vs->doc_view->document->timeout);
	install_timer(&interpreter->vs->doc_view->document->timeout, timeout, ecmascript_timeout_handler, interpreter);
}

static struct module *ecmascript_modules[] = {
#ifdef CONFIG_ECMASCRIPT_SEE
	&see_module,
#elif defined(CONFIG_ECMASCRIPT_SMJS)
	&spidermonkey_module,
#endif
	NULL,
};


struct module ecmascript_module = struct_module(
	/* name: */		N_("ECMAScript"),
	/* options: */		ecmascript_options,
	/* events: */		NULL,
	/* submodules: */	ecmascript_modules,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);
