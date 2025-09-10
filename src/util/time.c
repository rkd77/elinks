/** Time operations
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <time.h>

#include "elinks.h"

#include "osdep/osdep.h" /* For win32 gettimeofday() stub */
#include "util/error.h"
#include "util/time.h"

#ifdef CONFIG_OS_DOS
static void
uclock_to_timeval(timeval_T *t)
{
	t->sec = t->ticks / UCLOCKS_PER_SEC;
	t->usec = (t->ticks % UCLOCKS_PER_SEC) * 1000000L / UCLOCKS_PER_SEC;
}
#endif

/** Get the current time.
 * It attempts to use available functions, granularity
 * may be as worse as 1 second if time() is used.
 * @relates timeval_T */
timeval_T *
timeval_now(timeval_T *t)
{
	ELOG
#ifdef CONFIG_OS_DOS
	t->ticks = uclock();
#else
#ifdef HAVE_GETTIMEOFDAY
	struct timeval tv;

	gettimeofday(&tv, NULL);
	t->sec  = (long) tv.tv_sec;
	t->usec = (long) tv.tv_usec;
#else
#ifdef HAVE_CLOCK_GETTIME
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	t->sec  = (long) ts.tv_sec;
	t->usec = (long) ts.tv_nsec / 1000;
#else
	t->sec  = (long) time(NULL);
	t->usec = (long) 0;
#endif
#endif
#endif
	return t;
}

/** Subtract an interval to a timeval, it ensures that
 * result is never negative.
 * @relates timeval_T */
timeval_T *
timeval_sub_interval(timeval_T *t, timeval_T *interval)
{
	ELOG
#ifdef CONFIG_OS_DOS
	t->ticks -= interval->ticks;

	if (t->ticks < 0) {
		t->ticks = 0;
	}
#else
	t->sec  -= interval->sec;
	if (t->sec < 0) {
		t->sec = 0;
		t->usec = 0;
		return t;
	}

	t->usec -= interval->usec;

	while (t->usec < 0) {
		t->usec += 1000000;
		t->sec--;
	}

	if (t->sec < 0) {
		t->sec = 0;
		t->usec = 0;
	}
#endif
	return t;
}

/** @relates timeval_T */
timeval_T *
timeval_sub(timeval_T *res, timeval_T *older, timeval_T *newer)
{
	ELOG
#ifdef CONFIG_OS_DOS
	res->ticks = newer->ticks - older->ticks;
	if (res->ticks < 0) {
		res->ticks = 0;
	}
#else
	res->sec  = newer->sec - older->sec;
	res->usec = newer->usec - older->usec;

	while (res->usec < 0) {
		res->usec += 1000000;
		res->sec--;
	}
#endif
	return res;
}

/** @relates timeval_T */
timeval_T *
el_timeval_add(timeval_T *res, timeval_T *base, timeval_T *t)
{
	ELOG
#ifdef CONFIG_OS_DOS
	res->ticks = base->ticks + t->ticks;
#else
	res->sec  = base->sec + t->sec;
	res->usec = base->usec + t->usec;

	while (res->usec >= 1000000) {
		res->usec -= 1000000;
		res->sec++;
	}
#endif
	return res;
}

/** @relates timeval_T */
timeval_T *
timeval_add_interval(timeval_T *t, timeval_T *interval)
{
	ELOG
#ifdef CONFIG_OS_DOS
	t->ticks += interval->ticks;
#else
	t->sec  += interval->sec;
	t->usec += interval->usec;

	while (t->usec >= 1000000) {
		t->usec -= 1000000;
		t->sec++;
	}
#endif
	return t;
}

/** @relates timeval_T */
timeval_T *
timeval_from_double(timeval_T *t, double x)
{
	ELOG
#ifdef CONFIG_OS_DOS
	t->ticks = x * UCLOCKS_PER_SEC;
#else
	t->sec  = (long) x;
	t->usec = (long) ((x - (double) t->sec) * 1000000);
#endif
	return t;
}

/** @relates timeval_T */
timeval_T *
timeval_from_milliseconds(timeval_T *t, milliseconds_T milliseconds)
{
	ELOG
	long ms = (long) milliseconds;
#ifdef CONFIG_OS_DOS
	t->ticks = (ms / 1000.0) * UCLOCKS_PER_SEC;
#else
	t->sec = ms / 1000;
	t->usec = (ms % 1000) * 1000;
#endif
	return t;
}

/** @bug 923: Assumes time_t values fit in long.  (This function is used
 * for both timestamps and durations.)
 * @relates timeval_T */
timeval_T *
timeval_from_seconds(timeval_T *t, long seconds)
{
	ELOG
	t->sec = seconds;
	t->usec = 0;
#ifdef CONFIG_OS_DOS
	t->ticks = t->sec * UCLOCKS_PER_SEC;
#endif
	return t;
}

milliseconds_T
sec_to_ms(long sec)
{
	ELOG
	assert(sec >= 0 && sec < LONG_MAX / 1000L);
	if_assert_failed return (milliseconds_T) (LONG_MAX / 1000L);

	return (milliseconds_T) (sec * 1000L);
}

milliseconds_T
add_ms_to_ms(milliseconds_T a, milliseconds_T b)
{
	ELOG
	long la = (long) a;
	long lb = (long) b;

	assert(la >= 0 && lb >= 0 && lb < LONG_MAX - la);
	if_assert_failed return (milliseconds_T) (LONG_MAX / 1000L);

	return (milliseconds_T) (la + lb);
}

milliseconds_T
mult_ms(milliseconds_T a, long lb)
{
	ELOG
	long la = (long) a;

	assert(la >= 0 && lb >= 0 && la < LONG_MAX / lb);
	if_assert_failed return (milliseconds_T) (LONG_MAX / 1000L);

	return (milliseconds_T) (la * lb);
}

/** @relates timeval_T */
milliseconds_T
timeval_to_milliseconds(timeval_T *t)
{
	ELOG
#ifdef CONFIG_OS_DOS
	uclock_to_timeval(t);
#endif
	milliseconds_T a = sec_to_ms(t->sec);
	milliseconds_T b = (milliseconds_T) (t->usec / 1000L);

	return add_ms_to_ms(a, b);
}

/** @bug 923: Assumes time_t values fit in long.  (This function is used
 * for both timestamps and durations.)
 * @relates timeval_T */
long
timeval_to_seconds(timeval_T *t)
{
	ELOG
#ifdef CONFIG_OS_DOS
	return t->ticks / UCLOCKS_PER_SEC;
#else
	return t->sec + t->usec / 1000000L;
#endif
}

/** @relates timeval_T */
int
timeval_is_positive(timeval_T *t)
{
	ELOG
#ifdef CONFIG_OS_DOS
	return t->ticks > 0;
#else
	return (t->sec > 0 || (t->sec == 0 && t->usec > 0));
#endif
}

/** Be sure timeval is not negative.
 * @relates timeval_T */
void
timeval_limit_to_zero_or_one(timeval_T *t)
{
	ELOG
#ifdef CONFIG_OS_DOS
	if (t->ticks < 0) {
		t->ticks = 0;
	}
#else
	if (t->sec < 0) t->sec = 0;
	if (t->usec < 0) t->usec = 0;
#ifdef CONFIG_OS_WIN32
/* Under Windows I got 300 seconds timeout, so 1 second should not hurt --witekfl */
	if (t->sec > 1) t->sec = 1;
#endif
#endif
}

/** Compare time values.
 * @returns 1 if t1 > t2;
 * -1 if t1 < t2;
 * 0 if t1 == t2.
 * @relates timeval_T */
int
timeval_cmp(timeval_T *t1, timeval_T *t2)
{
	ELOG
#ifdef CONFIG_OS_DOS
	return t1->ticks - t2->ticks;
#else
	if (t1->sec > t2->sec) return 1;
	if (t1->sec < t2->sec) return -1;

	return t1->usec - t2->usec;
#endif
}

/** @relates timeval_T */
int
timeval_div_off_t(off_t n, timeval_T *t)
{
	ELOG
#ifdef CONFIG_OS_DOS
	uclock_to_timeval(t);
#endif
	longlong ln = 1000 * (longlong) n;	/* FIXME: off_t -> longlong ??? Find a better way. --Zas */
	longlong lsec = 1000 * (longlong) t->sec;
	int lusec = t->usec / 1000;

	if (lsec + lusec)
		return (ln / (lsec + lusec));
	else
		return INT_MAX;
}
