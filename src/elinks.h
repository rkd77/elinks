/* Global include with common functions and definitions for ELinks */

#ifndef EL__ELINKS_H
#define EL__ELINKS_H

#ifndef __EXTENSION__
#define __EXTENSION__ /* Helper for SunOS */
#endif

/* Gives us uint{32,16}_t and longlong and integer limits. */
#include "osdep/types.h"

/* This determines the system type and loads system-specific macros and
 * symbolic constants. The other includes may reuse this. This should be
 * always the very first ELinks include a source should include (except
 * config.h, of course). */
#include "osdep/system.h"

/* This introduces some generic ensurements that various things are how
 * they are supposed to be. */
#include "osdep/generic.h"

/* This loads hard-configured settings - which are too lowlevel to configure at
 * the runtime but are too unlikely to be changed to be configured through
 * config.h. */
#include "setup.h"

#ifdef CONFIG_DEBUG
#define DEBUG_MEMLEAK
#endif

/* This maybe overrides some of the standard high-level functions, to ensure
 * the expected behaviour. These overrides are not system specific. */
#include "osdep/stub.h"

/* util/math.h is supposed to be around all the time. */
#include "util/math.h"

#define C_(str) (char *)((str))

#define ELOG
#if 0
#define ELOG2 do { \
char outstr[200]; \
time_t t; \
struct tm *tmp; \
t = time(NULL); \
tmp = localtime(&t); \
if (tmp != NULL) { \
	if (strftime(outstr, sizeof(outstr), "%F-%T", tmp) != 0) { \
		fprintf(stderr, "%s %s\n", outstr, __FUNCTION__); \
	} \
} \
} while (0)
#endif

#ifdef CONFIG_OS_DOS
#define loop_select(a, b, c, d, e) dos_select(a, b, c, d, e, 1)
#define select2(a, b, c, d, e) dos_select(a, b, c, d, e, 0)
#else
#define loop_select select
#define select2 select
#endif

#endif
