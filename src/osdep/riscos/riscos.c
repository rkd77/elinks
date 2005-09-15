/* RISC OS system-specific routines. */
/* $Id: riscos.c,v 1.5 2004/08/14 23:00:08 jonas Exp $ */

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
