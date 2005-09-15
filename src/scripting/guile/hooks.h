/* $Id: hooks.h,v 1.5 2004/04/29 23:32:18 jonas Exp $ */

#ifndef EL__SCRIPTING_GUILE_HOOKS_H
#define EL__SCRIPTING_GUILE_HOOKS_H

#ifdef CONFIG_GUILE

#include "sched/event.h"

extern struct event_hook_info guile_scripting_hooks[];

#endif

#endif
