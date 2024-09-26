/* Ruby module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "intl/libintl.h"
#include "main/module.h"
#include "scripting/ruby/core.h"
#include "scripting/ruby/hooks.h"

static const char *
get_name_ruby(struct module *xxx)
{
	static char elrubyversion[32];
	snprintf(elrubyversion, 31, "Ruby %s", ruby_version);

	return elrubyversion;
}

struct module ruby_scripting_module = struct_module(
	/* name: */		N_("Ruby"),
	/* options: */		NULL,
	/* events: */		ruby_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_ruby,
	/* done: */		NULL,
	/* getname: */	get_name_ruby
);
