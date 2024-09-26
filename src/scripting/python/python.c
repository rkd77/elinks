/* Python module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "scripting/python/pythoninc.h"

#include "elinks.h"

#include "intl/libintl.h"
#include "main/module.h"
#include "scripting/python/core.h"
#include "scripting/python/hooks.h"


static const char *
get_name_python(struct module *xxx)
{
	static char elpythonversion[32];
	snprintf(elpythonversion, 31, "Python %s", PY_VERSION);

	return elpythonversion;
}

struct module python_scripting_module = struct_module(
	/* name: */		N_("Python"),
	/* options: */		NULL,
	/* hooks: */		python_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_python,
	/* done: */		cleanup_python,
	/* getname: */	get_name_python
);
