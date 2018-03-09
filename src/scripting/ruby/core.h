
#ifndef EL__SCRIPTING_RUBY_CORE_H
#define EL__SCRIPTING_RUBY_CORE_H

struct module;
struct session;

#include <ruby.h>	/* for VALUE */

#ifndef RSTRING_LEN
#define RSTRING_LEN(string) (RSTRING(string)->len)
#endif

#ifndef RSTRING_PTR
#define RSTRING_PTR(string) (RSTRING(string)->ptr)
#endif

#ifdef HAVE_RB_ERRINFO
#define RB_ERRINFO (rb_errinfo())
#else
#define RB_ERRINFO (ruby_errinfo)
#endif

VALUE erb_module;

void alert_ruby_error(struct session *ses, unsigned char *msg);
void erb_report_error(struct session *ses, int state);

void init_ruby(struct module *module);

#endif
