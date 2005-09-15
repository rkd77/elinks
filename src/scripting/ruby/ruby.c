/* Ruby module */
/* $Id: ruby.c,v 1.2 2005/06/13 00:43:29 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "main/module.h"
#include "scripting/ruby/core.h"
#include "scripting/ruby/hooks.h"


struct module ruby_scripting_module = struct_module(
	/* name: */		"Ruby",
	/* options: */		NULL,
	/* events: */		ruby_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_ruby,
	/* done: */		NULL
);
