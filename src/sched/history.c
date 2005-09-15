/* Visited URL history managment - NOT goto_url_dialog history! */
/* $Id: history.c,v 1.82 2004/06/25 09:54:46 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "dialogs/status.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "sched/history.h"
#include "sched/location.h"
#include "sched/session.h"
#include "sched/task.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


static inline void
free_history(struct list_head *history)
{
	while (!list_empty(*history)) {
		struct location *loc = history->next;

		del_from_list(loc);
		destroy_location(loc);
	}
}


void
create_history(struct ses_history *history)
{
	init_list(history->history);
	history->current = NULL;
}

void
destroy_history(struct ses_history *history)
{
	free_history(&history->history);
	history->current = NULL;
}

void
clean_unhistory(struct ses_history *history)
{
	if (!history->current) return;

	while (list_has_next(history->history, history->current)) {
		struct location *loc = history->current->next;

		del_from_list(loc);
		destroy_location(loc);
	}
}

/* If history->current points to an entry redundant to @loc, remove that
 * entry. */
#ifdef BUG_309_FIX
void
compress_history(struct ses_history *history, struct location *loc)
{
	struct location *current = history->current;

	assert(current);

	if (uris_compare(current->vs.uri, loc->vs.uri)
	    || (current->download.cached->redirect
		&& current->download.cached->redirect == loc->vs.uri)) {
		del_from_history(history, current);
		destroy_location(current);
	}
}
#endif


void
ses_history_move(struct session *ses)
{
	struct location *loc;

	/* Prepare. */

	free_files(ses);
	mem_free_set(&ses->search_word, NULL);

	/* Does it make sense? */

	if (!have_location(ses) || !ses->task.target_location)
		return;

	if (ses->task.target_location
	    == (struct location *) &ses->history.history)
		return;

	/* Move. */

	ses->history.current = ses->task.target_location;

	loc = cur_loc(ses);

	/* There can be only one ... */
	if (compare_uri(loc->vs.uri, ses->loading_uri, 0))
		return;

	/* Remake that location. */

    	del_from_history(&ses->history, loc);
	destroy_location(loc);
	ses_forward(ses, 0);

	/* Maybe trash the unhistory. */

	if (get_opt_bool("document.history.keep_unhistory"))
		clean_unhistory(&ses->history);
}


void
go_history(struct session *ses, struct location *loc)
{
	ses->reloadlevel = CACHE_MODE_NORMAL;

	if (ses->task.type) {
		abort_loading(ses, 0);
		print_screen_status(ses);
		reload(ses, CACHE_MODE_NORMAL);
		return;
	}

	if (!have_location(ses)
	    || loc == (struct location *) &ses->history.history) {
		/* There's no history, at most only the current location. */
		return;
	}

	abort_loading(ses, 0);

	set_session_referrer(ses, NULL);

	ses_goto(ses, loc->vs.uri, NULL, loc,
		 CACHE_MODE_ALWAYS, TASK_HISTORY, 0);
}
