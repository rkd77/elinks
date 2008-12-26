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


/* When CONFIG_UTF8 is defined, src/viewer/text/search.c makes a string
 * of unicode_val_T and gives it to regwexec(), which expects a string
 * of wchar_t.  If the unicode_val_T and wchar_t types are too different,
 * this won't work, so try to detect that and disable regexp operations
 * entirely in that case.
 *
 * Currently, this code only compares the sizes of the types.  Because
 * unicode_val_T is defined as uint32_t and POSIX says bytes are 8-bit,
 * sizeof(unicode_val_T) is 4 and the following compares SIZEOF_WCHAR_T
 * to that.
 *
 * C99 says the implementation can define __STDC_ISO_10646__ if wchar_t
 * values match ISO 10646 (or Unicode) numbers in all locales.  Do not
 * check that macro here, because it is too restrictive: it should be
 * enough for ELinks if the values match in the locales where ELinks is
 * actually run.  */

#ifdef CONFIG_UTF8
#if SIZEOF_WCHAR_T != 4
#undef HAVE_TRE_REGEX_H
#endif
#endif

/* This maybe overrides some of the standard high-level functions, to ensure
 * the expected behaviour. These overrides are not system specific. */
#include "osdep/stub.h"

/* util/math.h is supposed to be around all the time. */
#include "util/math.h"


#endif
