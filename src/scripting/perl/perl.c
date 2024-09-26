/* Perl module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "intl/libintl.h"
#include "main/module.h"
#include "scripting/perl/core.h"
#include "scripting/perl/hooks.h"

static const char *
get_name_perl(struct module *xxx)
{
	static char elperlversion[32];
	snprintf(elperlversion, 31, "Perl %s", PERL_VERSION_STRING);

	return elperlversion;
}

struct module perl_scripting_module = struct_module(
	/* name: */		N_("Perl"),
	/* options: */		NULL,
	/* hooks: */		perl_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_perl,
	/* done: */		cleanup_perl,
	/* getname: */	get_name_perl
);
