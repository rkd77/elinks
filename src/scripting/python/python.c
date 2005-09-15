/* Python module */
/* $Id: python.c,v 1.3 2005/06/13 00:43:29 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "scripting/python/core.h"

#include "elinks.h"

#include "main/module.h"
#include "scripting/python/hooks.h"


struct module python_scripting_module = struct_module(
	/* name: */		"Python",
	/* options: */		NULL,
	/* hooks: */		python_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_python,
	/* done: */		cleanup_python
);
