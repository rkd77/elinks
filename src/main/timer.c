/* Timers. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>

#if defined(HAVE_LIBEV) && !defined(OPENVMS) && !defined(DOS)
#ifdef HAVE_LIBEV_EVENT_H
#include <libev/event.h>
#elif defined(HAVE_EVENT_H)
#include <event.h>
#endif
#define USE_LIBEVENT
#endif

#if (defined(HAVE_EVENT_H) || defined(HAVE_EV_EVENT_H) || defined(HAVE_LIBEV_EVENT_H)) && defined(HAVE_LIBEVENT) && !defined(OPENVMS) && !defined(DOS)
#if defined(HAVE_EVENT_H)
#include <event.h>
#elif defined(HAVE_EV_EVENT_H)
#include <ev-event.h>
#endif
#define USE_LIBEVENT
#endif

#include "elinks.h"

#include "main/select.h"
#include "main/timer.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/time.h"

#ifdef CONFIG_ECMASCRIPT
#include "ecmascript/timer.h"
#endif

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

#ifdef HAVE_EVENT_BASE_SET
extern struct event_base *event_base;
#endif

#ifdef USE_LIBEVENT
extern int event_enabled;
#ifndef HAVE_EVENT_GET_STRUCT_EVENT_SIZE
#define sizeof_struct_event		sizeof(struct event)
#else
#define sizeof_struct_event		(event_get_struct_event_size())
#endif

static void
timer_callback(int h, short ev, void *data)
{
	struct timer *tm = data;
	tm->func(tm->data);
	kill_timer(&tm);
	check_bottom_halves();
}


static inline
struct event *timer_event(struct timer *tm)
{
	return (struct event *)((char *)tm - sizeof_struct_event);
}
#endif

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
#ifdef CONFIG_ECMASCRIPT
		del_from_map_timer(timer);
#endif
		/* At this point, *@timer is to be considered invalid
		 * outside timers.c; if anything e.g. passes it to
		 * @kill_timer, that's a bug.  However, @timer->func
		 * and @check_bottom_halves can still call @kill_timer
		 * on other timers, so this loop must be careful not to
		 * keep pointers to them.  (bug 868) */
		timer->func(timer->data);
#ifdef USE_LIBEVENT
		mem_free(timer_event(timer));
#else
		mem_free(timer);
#endif
		check_bottom_halves();
	}

	timeval_copy(last_time, &now);
}

#ifdef USE_LIBEVENT
static void
set_event_for_timer(timer_id_T tm)
{
	struct timeval tv;
	struct event *ev = timer_event(tm);
	timeout_set(ev, timer_callback, tm);
#ifdef HAVE_EVENT_BASE_SET
	if (event_base_set(event_base, ev) == -1)
		elinks_internal("ERROR: event_base_set failed: %s", strerror(errno));
#endif
	tv.tv_sec = tm->interval.sec;
	tv.tv_usec = tm->interval.usec;
#if defined(HAVE_LIBEV)
	if (!tm->interval.usec && ev_version_major() < 4) {
		/* libev bug */
		tv.tv_usec = 1;
	}
#endif
	if (timeout_add(ev, &tv) == -1)
		elinks_internal("ERROR: timeout_add failed: %s", strerror(errno));
}
#endif

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

#ifdef USE_LIBEVENT
	char *q = (char *)mem_alloc(sizeof_struct_event + sizeof(struct timer));
	new_timer = (struct timer *)(q + sizeof_struct_event);
#else
	new_timer = (struct timer *)mem_alloc(sizeof(*new_timer));
#endif
	*id = (timer_id_T) new_timer; /* TIMER_ID_UNDEF is NULL */
	if (!new_timer) return;

	timeval_from_milliseconds(&new_timer->interval, delay);
	new_timer->func = func;
	new_timer->data = data;
#ifdef USE_LIBEVENT
	if (event_enabled) {
		set_event_for_timer(new_timer);
		add_to_list(timers, new_timer);
	} else
#endif
	{
		foreach (timer, timers) {
			if (timeval_cmp(&timer->interval, &new_timer->interval) >= 0)
				break;
		}

		add_at_pos(timer->prev, new_timer);
	}
#ifdef CONFIG_ECMASCRIPT
	add_to_map_timer(new_timer);
#endif
}

void
kill_timer(timer_id_T *id)
{
	struct timer *timer;

	assert(id != NULL);
	if (*id == TIMER_ID_UNDEF) return;
	timer = *id;
	del_from_list(timer);
#ifdef CONFIG_ECMASCRIPT
	del_from_map_timer(timer);
#endif

#ifdef USE_LIBEVENT
	if (event_enabled) {
		timeout_del(timer_event(timer));
	}
	mem_free(timer_event(timer));
#else
	mem_free(timer);
#endif
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


void
set_events_for_timer(void)
{
#ifdef USE_LIBEVENT
	timer_id_T tm;

	foreach(tm, timers)
		set_event_for_timer(tm);
#endif
}
