/* Guile module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "intl/libintl.h"
#include "main/module.h"
#include "scripting/guile/core.h"
#include "scripting/guile/hooks.h"

static const char *
get_name_guile(struct module *xxx)
{
	static char elguileversion[32];
	snprintf(elguileversion, 31, "Guile %s", scm_to_locale_string(scm_version()));

	return elguileversion;
}

struct module guile_scripting_module = struct_module(
	/* name: */		N_("Guile"),
	/* options: */		NULL,
	/* events: */		guile_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_guile,
	/* done: */		NULL,
	/* getname: */	get_name_guile
);
