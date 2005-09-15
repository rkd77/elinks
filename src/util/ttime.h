/* $Id: ttime.h,v 1.8 2004/11/12 09:49:05 zas Exp $ */

#ifndef EL__UTIL_TTIME_H
#define EL__UTIL_TTIME_H

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

/* WARNING: may cause overflows since ttime holds values 1000 times
 * bigger than usual time_t */
typedef time_t ttime;

ttime get_time(void);

/* Is using atol() in this way acceptable? It seems
 * non-portable to me; ttime might not be a long. -- Miciah */
#define str_to_ttime(s) ((ttime) atol(s))

#endif
