/* $Id: core.h,v 1.2.2.1 2005/01/29 02:06:29 jonas Exp $ */

#ifndef EL__SCRIPTING_RUBY_CORE_H
#define EL__SCRIPTING_RUBY_CORE_H

struct session;

VALUE erb_module;

void alert_ruby_error(struct session *ses, unsigned char *msg);
void erb_report_error(struct session *ses, int state);

#endif
