/* Timers. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "main/select.h"
#include "main/timer.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/time.h"


struct timer {
	LIST_HEAD(struct timer);

	timeval_T interval;
	void (*func)(void *);
	void *data;
};

/* @timers.next points to the timer with the smallest interval,
 * @timers.next->next to the second smallest, and so on.  */
static INIT_LIST_OF(struct timer, timers);

int
get_timers_count(void)
{
	return list_size(&timers);

}

void
check_timers(timeval_T *last_time)
{
	timeval_T now;
	timeval_T interval;
	struct timer *timer;

	timeval_now(&now);
	timeval_sub(&interval, last_time, &now);

	foreach (timer, timers) {
		timeval_sub_interval(&timer->interval, &interval);
	}

	while (!list_empty(timers)) {
		timer = timers.next;

		if (timeval_is_positive(&timer->interval))
			break;

		del_from_list(timer);
		/* At this point, *@timer is to be considered invalid
		 * outside timers.c; if anything e.g. passes it to
		 * @kill_timer, that's a bug.  However, @timer->func
		 * and @check_bottom_halves can still call @kill_timer
		 * on other timers, so this loop must be careful not to
		 * keep pointers to them.  (bug 868) */
		timer->func(timer->data);
		mem_free(timer);
		check_bottom_halves();
	}

	timeval_copy(last_time, &now);
}

/* Install a timer that calls @func(@data) after @delay milliseconds.
 * Store to *@id either the ID of the new timer, or TIMER_ID_UNDEF if
 * the timer cannot be installed.  (This function ignores the previous
 * value of *@id in any case.)
 *
 * When @func is called, the timer ID has become invalid.  @func
 * should erase the expired timer ID from all variables, so that
 * there's no chance it will be given to @kill_timer later.  */
void
install_timer(timer_id_T *id, milliseconds_T delay, void (*func)(void *), void *data)
{
	struct timer *new_timer, *timer;

	assert(id && delay > 0);

	new_timer = mem_alloc(sizeof(*new_timer));
	*id = (timer_id_T) new_timer; /* TIMER_ID_UNDEF is NULL */
	if (!new_timer) return;

	timeval_from_milliseconds(&new_timer->interval, delay);
	new_timer->func = func;
	new_timer->data = data;

	foreach (timer, timers) {
		if (timeval_cmp(&timer->interval, &new_timer->interval) >= 0)
			break;
	}

	add_at_pos(timer->prev, new_timer);
}

void
kill_timer(timer_id_T *id)
{
	struct timer *timer;

	assert(id != NULL);
	if (*id == TIMER_ID_UNDEF) return;

	timer = *id;
	del_from_list(timer);
	mem_free(timer);

	*id = TIMER_ID_UNDEF;
}

int
get_next_timer_time(timeval_T *t)
{
	if (!list_empty(timers)) {
		timeval_copy(t, &((struct timer *) &timers)->next->interval);
		return 1;
	}

	return 0;
}
