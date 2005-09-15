/* Base ECMAScript file. Mostly a proxy for specific library backends. */
/* $Id: ecmascript.c,v 1.25.2.3 2005/04/05 21:08:41 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "config/options.h"
#include "document/document.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey.h"
#include "intl/gettext/libintl.h"
#include "modules/module.h"
#include "protocol/uri.h"
#include "sched/session.h"
#include "sched/task.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/conv.h"
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
		"enable", 0, 1,
		N_("Whether to run those scripts inside of documents.")),

	INIT_OPT_BOOL("ecmascript", N_("Script error reporting"),
		"error_reporting", 0, 0,
		N_("Open a message box when a script reports an error.")),

	INIT_OPT_INT("ecmascript", N_("Maximum execution time"),
		"max_exec_time", 0, 1, 3600, 5,
		N_("Maximum execution time in seconds for a script.")),

	INIT_OPT_BOOL("ecmascript", N_("Pop-up window blocking"),
		"block_window_opening", 0, 0,
		N_("Whether to disallow scripts to open new windows or tabs.")),

	NULL_OPTION_INFO,
};

#define get_ecmascript_enable()		get_opt_bool("ecmascript.enable")


static void
ecmascript_init(struct module *module)
{
	spidermonkey_init();
}

static void
ecmascript_done(struct module *module)
{
	spidermonkey_done();
}


struct ecmascript_interpreter *
ecmascript_get_interpreter(struct view_state *vs)
{
	struct ecmascript_interpreter *interpreter;

	assert(vs);

	interpreter = mem_calloc(1, sizeof(*interpreter));
	if (!interpreter)
		return NULL;

	interpreter->vs = vs;
	init_list(interpreter->onload_snippets);
	spidermonkey_get_interpreter(interpreter);

	return interpreter;
}

void
ecmascript_put_interpreter(struct ecmascript_interpreter *interpreter)
{
	assert(interpreter);
	spidermonkey_put_interpreter(interpreter);
	free_string_list(&interpreter->onload_snippets);
	mem_free(interpreter);
}

void
ecmascript_reset_state(struct view_state *vs)
{
	struct form_view *fv;
	int i;

	vs->ecmascript_fragile = 0;
	if (vs->ecmascript)
		ecmascript_put_interpreter(vs->ecmascript);

	foreach (fv, vs->forms)
		fv->ecmascript_obj = NULL;
	for (i = 0; i < vs->form_info_len; i++)
		vs->form_info[i].ecmascript_obj = NULL;

	vs->ecmascript = ecmascript_get_interpreter(vs);
	if (!vs->ecmascript)
		vs->ecmascript_fragile = 1;
}


void
ecmascript_eval(struct ecmascript_interpreter *interpreter,
                struct string *code)
{
	if (!get_ecmascript_enable())
		return;
	assert(interpreter);
	spidermonkey_eval(interpreter, code);
}


unsigned char *
ecmascript_eval_stringback(struct ecmascript_interpreter *interpreter,
			   struct string *code)
{
	if (!get_ecmascript_enable())
		return NULL;
	assert(interpreter);
	return spidermonkey_eval_stringback(interpreter, code);
}


int
ecmascript_eval_boolback(struct ecmascript_interpreter *interpreter,
			 struct string *code)
{
	if (!get_ecmascript_enable())
		return -1;
	assert(interpreter);
	return spidermonkey_eval_boolback(interpreter, code);
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


struct module ecmascript_module = struct_module(
	/* name: */		N_("ECMAScript"),
	/* options: */		ecmascript_options,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		ecmascript_init,
	/* done: */		ecmascript_done
);
