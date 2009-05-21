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


#endif
