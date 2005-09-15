/* $Id: hooks.h,v 1.1.2.1 2005/01/29 02:06:29 jonas Exp $ */

#ifndef EL__SCRIPTING_RUBY_HOOKS_H
#define EL__SCRIPTING_RUBY_HOOKS_H

#ifdef CONFIG_RUBY

#include "sched/event.h"

extern struct event_hook_info ruby_scripting_hooks[];

#endif

#endif
