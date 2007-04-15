/* Python module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Python.h>

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "scripting/python/core.h"
#include "scripting/python/hooks.h"


struct module python_scripting_module = struct_module(
	/* name: */		N_("Python"),
	/* options: */		NULL,
	/* hooks: */		python_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_python,
	/* done: */		cleanup_python
);
