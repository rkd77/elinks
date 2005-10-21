/* Ruby module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "main/module.h"
#include "scripting/ruby/core.h"
#include "scripting/ruby/hooks.h"


struct module ruby_scripting_module = struct_module(
	/* name: */		"Ruby",
	/* options: */		NULL,
	/* events: */		ruby_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_ruby,
	/* done: */		NULL
);
