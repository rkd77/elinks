/* $Id: core.h,v 1.3.6.1 2005/01/03 23:23:03 jonas Exp $ */

#ifndef EL__SCRIPTING_PERL_CORE_H
#define EL__SCRIPTING_PERL_CORE_H

#ifdef CONFIG_PERL

#include <EXTERN.h>
#include <perl.h>
#include <perlapi.h>

extern PerlInterpreter *my_perl;

#endif

#endif
