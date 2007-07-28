#ifndef EL__SESSION_HISTORY_H
#define EL__SESSION_HISTORY_H

struct location;
struct session;

struct ses_history {
	/** The first list item is the first visited location. The last list
	 * item is the last location in the unhistory. The #current location is
	 * included in this list. */
	LIST_OF(struct location) history;

	/** The current location. This is moveable pivot pointing somewhere at
	 * the middle of #history. */
	struct location *current;
};


void create_history(struct ses_history *history);
void destroy_history(struct ses_history *history);
void clean_unhistory(struct ses_history *history);

void add_to_history(struct ses_history *history, struct location *loc);
void del_from_history(struct ses_history *history, struct location *loc);


/** Note that this function is dangerous, and its results are sort of
 * unpredictable. If the document is cached and is permitted to be fetched from
 * the cache, the effect of this function is immediate and you end up with the
 * new location being cur_loc(). BUT if the cache entry cannot be used, the
 * effect is delayed to the next main loop iteration, as the TASK_HISTORY
 * session task (ses_history_move()) is executed not now but in the bottom-half
 * handler. So, you MUST NOT depend on cur_loc() having an arbitrary value
 * after call to this function (or the regents go_(un)back(), of course). */
void go_history(struct session *ses, struct location *loc);

/** Move back -@a n times if @a n is negative, forward @a n times if
 * positive. */
void go_history_by_n(struct session *ses, int n);

void go_back(struct session *ses);
void go_unback(struct session *ses);

void ses_history_move(struct session *ses);

#endif
