/** Visited URL history managment - NOT dialog_goto_url() history!
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "dialogs/status.h"
#include "network/connection.h"
#include "protocol/uri.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


static inline void
free_history(LIST_OF(struct location) *history)
{
	while (!list_empty(*history)) {
		struct location *loc = history->next;

		del_from_list(loc);
		destroy_location(loc);
	}
}


/** @relates ses_history */
void
create_history(struct ses_history *history)
{
	init_list(history->history);
	history->current = NULL;
}

/** @relates ses_history */
void
destroy_history(struct ses_history *history)
{
	free_history(&history->history);
	history->current = NULL;
}

/** @relates ses_history */
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

/** @relates ses_history */
void
add_to_history(struct ses_history *history, struct location *loc)
{
	if (!history->current) {
		add_to_list(history->history, loc);
	} else {
		add_at_pos(history->current, loc);
	}

	history->current = loc;
}

/** @relates ses_history */
void
del_from_history(struct ses_history *history, struct location *loc)
{
	if (history->current == loc)
		history->current = loc->prev;

	if (history->current == (struct location *) &history->history)
		history->current = loc->next;

	if (history->current == (struct location *) &history->history)
		history->current = NULL;
	del_from_list(loc);
}


void
ses_history_move(struct session *ses)
{
	struct location *loc;

	/* Prepare. */

	free_files(ses);
	mem_free_set(&ses->search_word, NULL);

	/* Does it make sense? */

	if (!have_location(ses) || !ses->task.target.location)
		return;

	if (ses->task.target.location
	    == (struct location *) &ses->history.history)
		return;

	/* Move. */

	ses->history.current = ses->task.target.location;

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

void
go_history_by_n(struct session *ses, int n)
{
	struct location *loc = cur_loc(ses);

	if (!loc) return;

	if (n > 0) {
		while (n-- && list_has_next(ses->history.history, loc))
			loc = loc->next;
	} else {
		while (n++ && list_has_prev(ses->history.history, loc))
			loc = loc->prev;
	}

	go_history(ses, loc);
}

/** Go backward in the history.  See go_history() description regarding
 * unpredictable effects on cur_loc() by this function. */
void
go_back(struct session *ses)
{
	go_history_by_n(ses, -1);
}

/** Go forward in the history.  See go_history() description regarding
 * unpredictable effects on cur_loc() by this function. */
void
go_unback(struct session *ses)
{
	go_history_by_n(ses, 1);
}
