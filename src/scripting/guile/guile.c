/* Guile module */
/* $Id: guile.c,v 1.2 2005/06/13 00:43:29 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "main/module.h"
#include "scripting/guile/core.h"
#include "scripting/guile/hooks.h"


struct module guile_scripting_module = struct_module(
	/* name: */		"Guile",
	/* options: */		NULL,
	/* events: */		guile_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_guile,
	/* done: */		NULL
);
