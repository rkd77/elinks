#ifndef EL__MAIN_TIMER_H
#define EL__MAIN_TIMER_H

#include "util/lists.h"
#include "util/time.h"

#ifdef __cplusplus
extern "C" {
#endif

struct timer {
	LIST_HEAD_EL(struct timer);

	timeval_T interval;
	void (*func)(void *);
	void *data;
};

/* Little hack, timer_id_T is in fact a pointer to the timer, so
 * it has to be of a pointer type.
 * The fact each timer is allocated ensure us that timer id will
 * be unique.
 * That way there is no need of id field in struct timer. --Zas */
typedef struct timer * timer_id_T;

/* Should always be NULL or you'll have to modify install_timer()
 * and kill_timer(). --Zas */
#define TIMER_ID_UNDEF ((timer_id_T) NULL)

int get_timers_count();
void check_timers(timeval_T *last_time);
void install_timer(timer_id_T *id, milliseconds_T delay, void (*)(void *), void *);
void kill_timer(timer_id_T *id);
int get_next_timer_time(timeval_T *t);
void set_events_for_timer(void);

#ifdef __cplusplus
}
#endif

#endif
