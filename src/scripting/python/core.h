
#ifndef EL__SCRIPTING_PYTHON_CORE_H
#define EL__SCRIPTING_PYTHON_CORE_H

struct module;
struct session;

void alert_python_error(struct session *ses);
void init_python(struct module *module);
void cleanup_python(struct module *module);

#endif
