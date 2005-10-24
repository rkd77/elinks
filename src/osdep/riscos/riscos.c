/* RISC OS system-specific routines. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "osdep/system.h"

#include "elinks.h"

#include "osdep/riscos/riscos.h"
#include "osdep/osdep.h"


int
is_xterm(void)
{
       return 1;
}
