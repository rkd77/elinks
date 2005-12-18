/* ECMAScript browser scripting module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "main/module.h"
#include "scripting/smjs/core.h"


struct module smjs_scripting_module = struct_module(
	/* name: */		"ECMAScript scripting engine",
	/* options: */		NULL,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_smjs,
	/* done: */		cleanup_smjs
);
