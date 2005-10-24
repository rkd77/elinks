/* Simple Ecmascript Engine (SEE) browser scripting module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "main/module.h"
#include "scripting/see/core.h"
#include "scripting/see/hooks.h"


struct module see_scripting_module = struct_module(
	/* name: */		"Simple Ecmascript Engine",
	/* options: */		NULL,
	/* events: */		see_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_see,
	/* done: */		NULL
);
