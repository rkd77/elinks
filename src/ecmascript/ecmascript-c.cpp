/* ECMAScript helper functions */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "dialogs/status.h"
#include "document/document.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/ecmascript-c.h"
#include "intl/libintl.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "session/task.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/view.h"

extern int interpreter_count;
extern int ecmascript_enabled;

int
ecmascript_get_interpreter_count(void)
{
	return interpreter_count;
}

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
