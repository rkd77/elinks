/* Simple Ecmascript Engine (SEE) browser scripting module */
/* $Id: see.c,v 1.2 2005/06/13 00:43:29 jonas Exp $ */

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
