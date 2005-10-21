
#ifndef EL__SCRIPTING_PERL_CORE_H
#define EL__SCRIPTING_PERL_CORE_H

#include <EXTERN.h>
#include <perl.h>
#include <perlapi.h>

struct module;

extern PerlInterpreter *my_perl;

void init_perl(struct module *module);
void cleanup_perl(struct module *module);

#endif
