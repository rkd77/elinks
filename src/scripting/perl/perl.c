/* Perl module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "scripting/perl/core.h"
#include "scripting/perl/hooks.h"


struct module perl_scripting_module = struct_module(
	/* name: */		N_("Perl"),
	/* options: */		NULL,
	/* hooks: */		perl_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_perl,
	/* done: */		cleanup_perl
);
