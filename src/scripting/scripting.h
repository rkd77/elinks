#ifndef EL__SCRIPTING_SCRIPTING_H
#define EL__SCRIPTING_SCRIPTING_H

#ifdef CONFIG_SCRIPTING

struct module;
struct session;

void
report_scripting_error(struct module *module, struct session *ses,
		       unsigned char *msg);

extern struct module scripting_module;

#endif

#endif
