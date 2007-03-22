/* Guile module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "scripting/guile/core.h"
#include "scripting/guile/hooks.h"


struct module guile_scripting_module = struct_module(
	/* name: */		N_("Guile"),
	/* options: */		NULL,
	/* events: */		guile_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_guile,
	/* done: */		NULL
);
