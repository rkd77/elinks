/* $Id: hooks.h,v 1.2 2004/04/29 23:32:19 jonas Exp $ */

#ifndef EL__SCRIPTING_PERL_HOOKS_H
#define EL__SCRIPTING_PERL_HOOKS_H

#ifdef CONFIG_PERL

#include "sched/event.h"

extern struct event_hook_info perl_scripting_hooks[];

#endif

#endif
