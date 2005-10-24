
#ifndef EL__SCRIPTING_RUBY_CORE_H
#define EL__SCRIPTING_RUBY_CORE_H

struct module;
struct session;

#include <ruby.h>	/* for VALUE */

VALUE erb_module;

void alert_ruby_error(struct session *ses, unsigned char *msg);
void erb_report_error(struct session *ses, int state);

void init_ruby(struct module *module);

#endif
