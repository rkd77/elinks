#ifndef EL__UTIL_TIME_H
#define EL__UTIL_TIME_H

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

typedef long milliseconds_T;
#define MILLISECONDS_MAX ((milliseconds_T) (LONG_MAX / 1000L))
#define ms_max(a, b) ((a) < (b) ? (b) : (a))
#define ms_min(a, b) ((a) < (b) ? (a) : (b))

/** @bug 923: Assumes time_t values fit in @c long.  */
#define str_to_time_t(s) ((time_t) atol(s))
/** When formatting time_t values to be parsed with str_to_time_t(),
 * we first cast to @c time_print_T and then printf() the result with
 * ::TIME_PRINT_FORMAT.
 * @bug 923: Assumes time_t values fit in @c long.  */
typedef long time_print_T;
#define TIME_PRINT_FORMAT "ld"	/**< @see time_print_T */

/** Redefine a timeval that has all fields signed so calculations
 * will be simplified on rare systems that define timeval with
 * unsigned fields.
 * @bug 923: Assumes time_t values fit in long.  (This structure is
 * used for both timestamps and durations.)  */
typedef struct { long sec; long usec; } timeval_T;

timeval_T *timeval_from_milliseconds(timeval_T *t, milliseconds_T milliseconds);
timeval_T *timeval_from_seconds(timeval_T *t, long seconds);
timeval_T *timeval_from_double(timeval_T *t, double x);

milliseconds_T sec_to_ms(long sec);
milliseconds_T add_ms_to_ms(milliseconds_T a, milliseconds_T b);
milliseconds_T mult_ms(milliseconds_T a, long lb);

milliseconds_T timeval_to_milliseconds(timeval_T *t);
long timeval_to_seconds(timeval_T *t);

int timeval_is_positive(timeval_T *t);
void timeval_limit_to_zero_or_one(timeval_T *t);
timeval_T *timeval_now(timeval_T *t);
timeval_T *timeval_sub(timeval_T *res, timeval_T *older, timeval_T *newer);
timeval_T *timeval_add(timeval_T *res, timeval_T *base, timeval_T *t);
int timeval_cmp(timeval_T *t1, timeval_T *t2);
timeval_T *timeval_sub_interval(timeval_T *t, timeval_T *interval);
timeval_T *timeval_add_interval(timeval_T *t, timeval_T *interval);
int timeval_div_off_t(off_t n, timeval_T *t);

/** @relates timeval_T */
#define timeval_copy(dst, src) copy_struct(dst, src)

#endif
