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
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/conv.h"
#include "viewer/text/view.h" /* current_frame() */
#include "viewer/text/form.h" /* <-ecmascript_reset_state() */
#include "viewer/text/vs.h"

#ifdef CONFIG_ECMASCRIPT_SEE
#include "ecmascript/see.h"
#elif defined(CONFIG_ECMASCRIPT_SMJS)
#include "ecmascript/spidermonkey.h"
#endif


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

	INIT_OPT_BOOL("ecmascript", N_("Ignore <noscript> content"),
		"ignore_noscript", 0, 0,
		N_("Whether to ignore content enclosed by the <noscript> tag\n"
                   "when ECMAScript is enabled.")),

	INIT_OPT_INT("ecmascript", N_("Maximum execution time"),
		"max_exec_time", 0, 1, 3600, 5,
		N_("Maximum execution time in seconds for a script.")),

	INIT_OPT_BOOL("ecmascript", N_("Pop-up window blocking"),
		"block_window_opening", 0, 0,
		N_("Whether to disallow scripts to open new windows or tabs.")),

	NULL_OPTION_INFO,
};

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
	/* init: */		ecmascript_init,
	/* done: */		ecmascript_done
);
