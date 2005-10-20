/* General scripting system functionality */
/* $Id: scripting.c,v 1.22 2005/06/13 00:43:29 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "scripting/scripting.h"


/* Backends dynamic area: */

#include "scripting/guile/guile.h"
#include "scripting/lua/lua.h"
#include "scripting/perl/perl.h"
#include "scripting/python/python.h"
#include "scripting/ruby/ruby.h"
#include "scripting/see/see.h"


static struct module *scripting_modules[] = {
#ifdef CONFIG_LUA
	&lua_scripting_module,
#endif
#ifdef CONFIG_GUILE
	&guile_scripting_module,
#endif
#ifdef CONFIG_PERL
	&perl_scripting_module,
#endif
#ifdef CONFIG_PYTHON
	&python_scripting_module,
#endif
#ifdef CONFIG_RUBY
	&ruby_scripting_module,
#endif
#ifdef CONFIG_SEE
	&see_scripting_module,
#endif
	NULL,
};

struct module scripting_module = struct_module(
	/* name: */		N_("Scripting"),
	/* options: */		NULL,
	/* events: */		NULL,
	/* submodules: */	scripting_modules,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);
